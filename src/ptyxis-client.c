/*
 * ptyxis-client.c
 *
 * Copyright 2023 Christian Hergert <christian@sourceandstack.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <errno.h>
#include <sys/ioctl.h>
#ifdef __linux__
# include <sys/prctl.h>
#endif
#include <sys/socket.h>

#include <glib/gstdio.h>
#include <glib-unix.h>

#include "ptyxis-application.h"
#include "ptyxis-client.h"
#include "ptyxis-client-private.h"
#include "ptyxis-dbus-util.h"
#include "ptyxis-process-private.h"
#include "ptyxis-util.h"

struct _PtyxisClient
{
  GObject          parent_instance;
  GPtrArray       *containers;
  GSubprocess     *subprocess;
  GDBusConnection *bus;
  PtyxisIpcAgent  *proxy;
  GHashTable      *processes;
  GHashTable      *early_exits;
  guint            spawning;
  guint64          containers_generation;
  gboolean         loading_containers;
};

enum {
  PROP_0,
  PROP_N_ITEMS,
  N_PROPS
};

enum {
  CLOSED,
  N_SIGNALS
};

static GType
ptyxis_client_get_item_type (GListModel *model)
{
  return PTYXIS_IPC_TYPE_CONTAINER;
}

static guint
ptyxis_client_get_n_items (GListModel *model)
{
  return PTYXIS_CLIENT (model)->containers->len;
}

static gpointer
ptyxis_client_get_item (GListModel *model,
                        guint       position)
{
  PtyxisClient *self = PTYXIS_CLIENT (model);

  if (position < self->containers->len)
    return g_object_ref (g_ptr_array_index (self->containers, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ptyxis_client_get_item_type;
  iface->get_n_items = ptyxis_client_get_n_items;
  iface->get_item = ptyxis_client_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PtyxisClient, ptyxis_client, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
ptyxis_client_dispose (GObject *object)
{
  PtyxisClient *self = (PtyxisClient *)object;
  GHashTableIter iter;
  gpointer exited;

  g_hash_table_iter_init (&iter, self->processes);
  while (g_hash_table_iter_next (&iter, NULL, &exited))
    {
      if (dex_future_is_pending (exited))
        dex_promise_reject (exited,
                            g_error_new_literal (G_IO_ERROR,
                                                 G_IO_ERROR_CLOSED,
                                                 "The client was closed"));
    }

  g_hash_table_remove_all (self->processes);
  if (self->subprocess != NULL)
    g_subprocess_force_exit (self->subprocess);

  if (self->containers->len > 0)
    g_ptr_array_remove_range (self->containers, 0, self->containers->len);

  g_clear_object (&self->bus);
  g_clear_object (&self->proxy);
  g_clear_object (&self->subprocess);

  G_OBJECT_CLASS (ptyxis_client_parent_class)->dispose (object);
}

static void
ptyxis_client_finalize (GObject *object)
{
  PtyxisClient *self = (PtyxisClient *)object;

  g_clear_pointer (&self->containers, g_ptr_array_unref);
  g_clear_pointer (&self->processes, g_hash_table_unref);
  g_clear_pointer (&self->early_exits, g_hash_table_unref);

  G_OBJECT_CLASS (ptyxis_client_parent_class)->finalize (object);
}

static void
ptyxis_client_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  PtyxisClient *self = PTYXIS_CLIENT (object);

  switch (prop_id)
    {
    case PROP_N_ITEMS:
      g_value_set_uint (value, self->containers->len);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_client_class_init (PtyxisClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_client_dispose;
  object_class->finalize = ptyxis_client_finalize;
  object_class->get_property = ptyxis_client_get_property;

  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT-1, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[CLOSED] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);


}

static void
ptyxis_client_init (PtyxisClient *self)
{
  self->containers = g_ptr_array_new_with_free_func (g_object_unref);
  self->processes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, dex_unref);
  self->early_exits = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
ptyxis_client_process_exited_cb (PtyxisClient   *self,
                                 const char     *process_object_path,
                                 int             exit_code,
                                 PtyxisIpcAgent *agent)
{
  g_assert (PTYXIS_IS_CLIENT (self));
  g_assert (PTYXIS_IPC_IS_AGENT (agent));

  if (g_hash_table_contains (self->processes, process_object_path))
    {
      DexPromise *exited = g_hash_table_lookup (self->processes, process_object_path);

      if (dex_future_is_pending (DEX_FUTURE (exited)))
        dex_promise_resolve_int (exited, exit_code);

      g_hash_table_remove (self->processes, process_object_path);
    }
  else if (self->spawning > 0)
    {
      g_hash_table_insert (self->early_exits, g_strdup (process_object_path), GINT_TO_POINTER (exit_code));
    }
}

static DexFuture *
ptyxis_client_load_containers (gpointer data)
{
  PtyxisClient *self = data;
  g_autoptr(GError) error = NULL;

  self->loading_containers = TRUE;

  for (;;)
    {
      g_autoptr(PtyxisIpcAgentListContainersResult) result = NULL;
      g_autoptr(GPtrArray) containers = g_ptr_array_new_with_free_func (g_object_unref);
      guint64 generation = self->containers_generation;
      guint removed;
      guint position = 0;
      guint suffix = 0;

      if (!(result = dex_await_boxed (ptyxis_ipc_agent_call_list_containers_future (self->proxy), &error)))
        break;

      for (guint i = 0; result->containers[i] != NULL; i++)
        {
          PtyxisIpcContainer *container = NULL;

          for (guint j = 0; j < self->containers->len; j++)
            {
              PtyxisIpcContainer *existing = g_ptr_array_index (self->containers, j);

              if (g_str_equal (result->containers[i], g_dbus_proxy_get_object_path (G_DBUS_PROXY (existing))))
                {
                  container = g_object_ref (existing);
                  break;
                }
            }

          if (container == NULL)
            container = dex_await_object (ptyxis_ipc_container_proxy_new_future (self->bus,
                                                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                                                 NULL,
                                                                                 result->containers[i]),
                                          &error);

          if (container == NULL)
            break;

          g_ptr_array_add (containers, container);
        }

      if (generation != self->containers_generation)
        {
          g_clear_error (&error);
          continue;
        }

      if (error != NULL)
        break;

      removed = self->containers->len;
      while (position < removed && position < containers->len &&
             self->containers->pdata[position] == containers->pdata[position])
        position++;
      while (suffix < removed - position && suffix < containers->len - position &&
             self->containers->pdata[removed - suffix - 1] == containers->pdata[containers->len - suffix - 1])
        suffix++;

      g_ptr_array_unref (self->containers);
      self->containers = g_steal_pointer (&containers);

      self->loading_containers = FALSE;

      if (removed - position - suffix > 0 || self->containers->len - position - suffix > 0)
        g_list_model_items_changed (G_LIST_MODEL (self),
                                    position,
                                    removed - position - suffix,
                                    self->containers->len - position - suffix);

      if (removed != self->containers->len)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);

      return dex_future_new_true ();
    }

  self->loading_containers = FALSE;

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static DexFuture *
ptyxis_client_containers_failed (DexFuture *future,
                                 gpointer   data)
{
  g_autoptr(GError) error = NULL;

  dex_future_get_value (future, &error);
  g_warning ("Failed to refresh containers: %s", error->message);
  return NULL;
}

static void
ptyxis_client_containers_changed_cb (PtyxisClient       *self,
                                     guint               position,
                                     guint               removed,
                                     const char * const *added,
                                     PtyxisIpcAgent     *agent)
{
  g_assert (PTYXIS_IS_CLIENT (self));

  self->containers_generation++;

  if (!self->loading_containers)
    {
      self->loading_containers = TRUE;
      dex_future_disown (dex_future_catch (dex_scheduler_spawn (NULL,
                                                                0,
                                                                ptyxis_client_load_containers,
                                                                g_object_ref (self),
                                                                g_object_unref),
                                            ptyxis_client_containers_failed,
                                            NULL,
                                            NULL));
    }
}

static char *
find_ptyxis_agent_path (gboolean in_sandbox)
{
  if (!in_sandbox &&
      ptyxis_get_process_kind () == PTYXIS_PROCESS_KIND_FLATPAK)
    {
      g_autoptr(GFile) file = g_file_new_for_path ("/.flatpak-info");
      g_autoptr(GBytes) bytes = dex_await_boxed (dex_file_load_contents_bytes (file), NULL);

      if (bytes != NULL)
        {
          g_autoptr(GKeyFile) key_file = g_key_file_new ();
          g_autofree char *str = NULL;

          if (g_key_file_load_from_bytes (key_file, bytes, 0, NULL) &&
              (str = g_key_file_get_string (key_file, "Instance", "app-path", NULL)))
            return g_build_filename (str, "libexec", "ptyxis-agent", NULL);
        }
    }

  return g_build_filename (LIBEXECDIR, "ptyxis-agent", NULL);
}

static void
ptyxis_client_closed_cb (PtyxisClient    *self,
                         gboolean         remote_peer_vanished,
                         GError          *error,
                         GDBusConnection *connection)
{
  GHashTableIter iter;
  gpointer process;

  g_assert (PTYXIS_IS_CLIENT (self));

  g_hash_table_iter_init (&iter, self->processes);
  while (g_hash_table_iter_next (&iter, NULL, &process))
    {
      if (dex_future_is_pending (process))
        dex_promise_reject (process,
                            g_error_new_literal (G_IO_ERROR,
                                                 G_IO_ERROR_CLOSED,
                                                 "The connection to the agent has closed"));
    }

  g_hash_table_remove_all (self->processes);
  g_hash_table_remove_all (self->early_exits);

  g_signal_emit (self, signals[CLOSED], 0);
}

static void
ptyxis_client_child_setup_func (gpointer data)
{
  setsid ();
  setpgid (0, 0);

#ifdef __linux__
  prctl (PR_SET_PDEATHSIG, SIGKILL);
#endif
}

DEX_DEFINE_CLOSURE_TYPE (ClientConnect, client_connect,
                         DEX_DEFINE_CLOSURE_VALUE (gboolean, in_sandbox),
                         DEX_DEFINE_CLOSURE_POINTER (char *, agent_path, g_free))

static DexFuture *
ptyxis_client_connect (gpointer data)
{
  ClientConnect *connect = data;
  gboolean in_sandbox = connect->in_sandbox;
  g_autofree char *ptyxis_agent_path = connect->agent_path
                                     ? g_strdup (connect->agent_path) : find_ptyxis_agent_path (in_sandbox);
  g_autoptr(PtyxisClient) self = g_object_new (PTYXIS_TYPE_CLIENT, NULL);
  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (0);
  g_autoptr(GPtrArray) argv = g_ptr_array_new_with_free_func (g_free);
  g_autoptr(GSocketConnection) stream = NULL;
  g_autoptr(GSocket) socket = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *guid = NULL;
  gint64 default_rlimit_nofile;
  int pair[2];
  int res;

  if (!in_sandbox &&
      ptyxis_get_process_kind () == PTYXIS_PROCESS_KIND_FLATPAK)
    {
      g_ptr_array_add (argv, g_strdup ("flatpak-spawn"));
      g_ptr_array_add (argv, g_strdup ("--host"));
      g_ptr_array_add (argv, g_strdup ("--watch-bus"));
      g_ptr_array_add (argv, g_strdup_printf ("--forward-fd=3"));
    }

  g_ptr_array_add (argv, g_strdup (ptyxis_agent_path));
  g_ptr_array_add (argv, g_strdup ("--socket-fd=3"));
  if ((default_rlimit_nofile = ptyxis_application_get_default_rlimit_nofile ()) > 0)
    g_ptr_array_add (argv, g_strdup_printf ("--rlimit-nofile=%"G_GINT64_FORMAT, default_rlimit_nofile));
  g_ptr_array_add (argv, NULL);

#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  res = socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0, pair);
#else
  res = socketpair (AF_UNIX, SOCK_STREAM, 0, pair);

  if (res == 0)
    {
      fcntl (pair[0], F_SETFD, FD_CLOEXEC);
      fcntl (pair[1], F_SETFD, FD_CLOEXEC);
      g_unix_set_fd_nonblocking (pair[0], TRUE, NULL);
      g_unix_set_fd_nonblocking (pair[1], TRUE, NULL);
    }
#endif

  if (res != 0)
    {
      int errsv = errno;

      g_set_error_literal (&error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      goto failure;
    }

  if (!(socket = g_socket_new_from_fd (pair[0], &error)))
    {
      close (pair[0]);
      close (pair[1]);

      goto failure;
    }

  g_subprocess_launcher_take_fd (launcher, pair[1], 3);
  g_subprocess_launcher_unsetenv (launcher, "SYSPROF_CONTROL_FD");

  pair[0] = -1;
  pair[1] = -1;

  g_subprocess_launcher_set_child_setup (launcher,
                                         ptyxis_client_child_setup_func,
                                         NULL,
                                         NULL);
  if (!(self->subprocess = g_subprocess_launcher_spawnv (launcher,
                                                         (const char * const *)argv->pdata,
                                                         &error)))
    goto failure;

  guid = g_dbus_generate_guid ();
  stream = g_socket_connection_factory_create_connection (socket);
  self->bus = dex_await_object (ptyxis_dbus_connection_new_server (G_IO_STREAM (stream), guid, 1),
                                &error);
  if (self->bus == NULL)
    goto failure;

  g_signal_connect_object (self->bus,
                           "closed",
                           G_CALLBACK (ptyxis_client_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->proxy = dex_await_object (ptyxis_ipc_agent_proxy_new_future (self->bus,
                                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                                     NULL,
                                                                     "/org/gnome/Ptyxis/Agent"),
                                  &error);
  if (self->proxy == NULL)
    goto failure;

  self->loading_containers = TRUE;

  g_signal_connect_object (self->proxy,
                           "containers-changed",
                           G_CALLBACK (ptyxis_client_containers_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->proxy,
                           "process-exited",
                           G_CALLBACK (ptyxis_client_process_exited_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (!dex_await (dex_dbus_connection_call (self->bus,
                                            NULL,
                                            "/org/gnome/Ptyxis/Agent",
                                            "org.freedesktop.DBus.Peer",
                                            "Ping",
                                            NULL,
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            in_sandbox ? G_MAXINT : 2000),
                  &error) ||
      !dex_await (ptyxis_client_load_containers (self), &error))
    goto failure;

  if (g_dbus_connection_is_closed (self->bus))
    {
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           "The agent disconnected during initialization");
      goto failure;
    }

  return dex_future_new_take_object (g_steal_pointer (&self));

failure:
  if (self->subprocess != NULL)
    {
      g_subprocess_force_exit (self->subprocess);
      dex_await (dex_subprocess_wait (self->subprocess), NULL);
    }

  if (self->bus != NULL)
    dex_await (dex_dbus_connection_close (self->bus), NULL);

  return dex_future_new_for_error (g_steal_pointer (&error));
}

/**
 * ptyxis_client_new:
 * @in_sandbox: whether to launch the agent within the application sandbox
 *
 * Connects to an agent and loads its initial container snapshot.
 *
 * Returns: (transfer full): a future resolving to a fully initialized #PtyxisClient
 */
DexFuture *
ptyxis_client_new (gboolean in_sandbox)
{
  return _ptyxis_client_new_for_agent (in_sandbox, NULL);
}

DexFuture *
_ptyxis_client_new_for_agent (gboolean    in_sandbox,
                              const char *agent_path)
{
  ClientConnect *connect = client_connect_new ();

  connect->in_sandbox = in_sandbox;
  connect->agent_path = g_strdup (agent_path);

  return dex_scheduler_spawn (NULL,
                              0,
                              ptyxis_client_connect,
                              connect,
                              (GDestroyNotify)client_connect_free);
}

void
ptyxis_client_force_exit (PtyxisClient *self)
{
  g_return_if_fail (PTYXIS_IS_CLIENT (self));

  g_subprocess_force_exit (self->subprocess);
}

static DexFuture *
ptyxis_client_create_pty_result (DexFuture *future,
                                 gpointer   data)
{
  const PtyxisIpcAgentCreatePtyResult *result = g_value_get_boxed (dex_future_get_value (future, NULL));
  g_autoptr(GError) error = NULL;
  VtePty *pty;
  int fd;

  if ((fd = g_unix_fd_list_get (result->fd_list, g_variant_get_handle (result->fd), &error)) == -1)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(pty = vte_pty_new_foreign_sync (fd, NULL, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  vte_pty_set_utf8 (pty, TRUE, NULL);

  return dex_future_new_take_object (pty);
}

/**
 * ptyxis_client_create_pty:
 * @self: a connected client
 *
 * Returns: (transfer full): a future resolving to a #VtePty owning the received FD
 */
DexFuture *
ptyxis_client_create_pty (PtyxisClient *self)
{
  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);

  return dex_future_then (ptyxis_ipc_agent_call_create_pty_future (self->proxy, NULL),
                          ptyxis_client_create_pty_result,
                          NULL,
                          NULL);
}

DEX_DEFINE_CLOSURE_TYPE (Spawn, spawn,
                         DEX_DEFINE_CLOSURE_OBJECT (PtyxisClient, client),
                         DEX_DEFINE_CLOSURE_OBJECT (PtyxisIpcContainer, container),
                         DEX_DEFINE_CLOSURE_OBJECT (PtyxisProfile, profile),
                         DEX_DEFINE_CLOSURE_OBJECT (VtePty, pty),
                         DEX_DEFINE_CLOSURE_POINTER (char *, tab_uuid, g_free),
                         DEX_DEFINE_CLOSURE_POINTER (char *, directory, g_free),
                         DEX_DEFINE_CLOSURE_POINTER (char **, argv, g_strfreev),
                         DEX_DEFINE_CLOSURE_POINTER (char **, env, g_strfreev),
                         DEX_DEFINE_CLOSURE_POINTER (DexPromise *, promise, dex_unref))

static const char *
maybe_extract_path_from_uri (GFile      *file,
                             const char *original_uri)
{
  if (file == NULL)
    return NULL;

  /*
   * Special-case handling for URIs which are file:// but GVFS
   * translated them into something like `sftp://.../` even though
   * they are local on disk (`/run/user/1000/gvfs/.../`).
   */
  if (!g_file_is_native (file))
    {
      if (original_uri == NULL)
        return NULL;

      if (!(original_uri[0] != '/' || g_str_has_prefix (original_uri, "file://")))
        return NULL;
    }

  return g_file_peek_path (file);
}

static DexFuture *
ptyxis_client_spawn_fiber (gpointer data)
{
  Spawn *spawn = data;
  PtyxisClient *self = spawn->client;
  PtyxisIpcContainer *container = spawn->container;
  PtyxisProfile *profile = spawn->profile;
  const char *tab_uuid = spawn->tab_uuid;
  const char *last_working_directory_uri = spawn->directory;
  const char * const *alt_argv = (const char * const *)spawn->argv;
  const char * const *alt_env = (const char * const *)spawn->env;
  g_autoptr(PtyxisIpcAgentGetPreferredShellResult) shell = NULL;
  g_autoptr(PtyxisIpcContainerFindProgramInPathResult) shell_path = NULL;
  g_autoptr(PtyxisIpcAgentCreatePtyProducerResult) producer = NULL;
  g_autoptr(PtyxisIpcContainerSpawnResult) result = NULL;
  g_autoptr(PtyxisIpcProcess) proxy = NULL;
  g_autoptr(PtyxisProcess) process = NULL;
  g_autofree char *shell_base = NULL;
  const char *default_shell;
  g_autofree char *custom_command = NULL;
  g_autofree char *arg0 = NULL;
  g_autoptr(GVariantBuilder) fd_builder = NULL;
  g_autoptr(GVariantBuilder) env_builder = NULL;
  g_autoptr(GStrvBuilder) argv_builder = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) last_directory = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_auto(GStrv) env = NULL;
  g_auto(GStrv) full_argv = NULL;
  g_autofd int pty_fd = -1;
  const char *cwd = NULL;
  gboolean preserve_directory_safe = FALSE;
  char vte_version[32];
  int handle;

  shell = dex_await_boxed (ptyxis_ipc_agent_call_get_preferred_shell_future (self->proxy), NULL);
  shell_base = g_path_get_basename (shell != NULL && !ptyxis_str_empty0 (shell->shell)
             ? shell->shell : "bash");
  shell_path = dex_await_boxed (ptyxis_ipc_container_call_find_program_in_path_future (container, shell_base), NULL);
  default_shell = shell_path != NULL && !ptyxis_str_empty0 (shell_path->path)
                ? shell_path->path : NULL;

  fd_list = g_unix_fd_list_new ();
  if ((handle = g_unix_fd_list_append (fd_list, vte_pty_get_fd (spawn->pty), &error)) == -1)
    goto failure;

  producer = dex_await_boxed (ptyxis_ipc_agent_call_create_pty_producer_future (self->proxy,
                                                                                g_variant_new_handle (handle),
                                                                                fd_list),
                              &error);
  if (producer == NULL ||
      (pty_fd = g_unix_fd_list_get (producer->fd_list,
                                    g_variant_get_handle (producer->fd),
                                    &error)) == -1)
    goto failure;

  g_clear_object (&fd_list);

  /* Make sure that the child PTY FD is blocking as most things
   * will expect that by default. We continue to keep our consumer
   * FD non-blocking for how we use it.
   */
  if (!g_unix_set_fd_nonblocking (pty_fd, FALSE, &error))
    {
      g_warning ("Failed to set child PTY FD non-blocking: %s",
                 error->message);
      g_clear_error (&error);
    }

  if (ptyxis_profile_get_use_proxy (profile))
    {
      g_autoptr(PtyxisIpcAgentDiscoverProxyEnvironmentResult) proxy_env = NULL;

      proxy_env = dex_await_boxed (ptyxis_ipc_agent_call_discover_proxy_environment_future (self->proxy), NULL);
      if (proxy_env != NULL)
        env = g_steal_pointer (&proxy_env->proxy_environment);
    }

  env = g_environ_setenv (env, "PTYXIS_PROFILE", ptyxis_profile_get_uuid (profile), TRUE);
  if (tab_uuid != NULL)
    env = g_environ_setenv (env, "PTYXIS_TAB", tab_uuid, TRUE);
  env = g_environ_setenv (env, "PTYXIS_VERSION", PACKAGE_VERSION, TRUE);
  env = g_environ_setenv (env, "COLORTERM", "truecolor", TRUE);
  env = g_environ_setenv (env, "TERM", "xterm-256color", TRUE);

  g_snprintf (vte_version, sizeof vte_version, "%u", VTE_VERSION_NUMERIC);
  env = g_environ_setenv (env, "VTE_VERSION", vte_version, TRUE);

  for (guint i = 0; alt_env != NULL && alt_env[i] != NULL; i++)
    {
      const char *pair = alt_env[i];
      const char *eq = strchr (pair, '=');
      const char *val = eq ? eq + 1 : "";
      g_autofree char *key = eq ? g_strndup (pair, eq - pair) : g_strdup (pair);

      env = g_environ_setenv (env, key, val, TRUE);
    }

  argv_builder = g_strv_builder_new ();

  if (alt_argv != NULL && alt_argv[0] != NULL)
    {
      arg0 = NULL;
      g_strv_builder_addv (argv_builder, (const char **)alt_argv);
    }
  else if (ptyxis_profile_get_use_custom_command (profile))
    {
      g_auto(GStrv) argv = NULL;

      custom_command = ptyxis_profile_dup_custom_command (profile);

      if (!g_shell_parse_argv (custom_command, NULL, &argv, &error))
        {
          goto failure;
        }

      g_strv_builder_addv (argv_builder, (const char **)argv);
      arg0 = g_strdup (argv[0]);
    }
  else if (default_shell != NULL)
    {
      arg0 = g_strdup (default_shell);
      g_strv_builder_add (argv_builder, arg0);
    }
  else
    {
      preserve_directory_safe = TRUE;
      arg0 = NULL;
      g_strv_builder_add (argv_builder, "sh");
      g_strv_builder_add (argv_builder, "-c");
      g_strv_builder_add (argv_builder, "if [ -x \"$(getent passwd $(whoami) | cut -d : -f 7)\" ]; then exec $(getent passwd $(whoami) | cut -d : -f 7); else exec sh; fi");
    }

  /* TODO: We might want to check with the container that this
   * is a shell (as opposed to one available on the host).
   */
  preserve_directory_safe |= (arg0 != NULL) && ptyxis_is_shell (arg0);

  if (arg0 != NULL &&
      ptyxis_profile_get_login_shell (profile) &&
      ptyxis_shell_supports_dash_l (arg0))
    g_strv_builder_add (argv_builder, "-l");

  if (last_working_directory_uri != NULL)
    last_directory = g_file_new_for_uri (last_working_directory_uri);

  if (alt_argv != NULL && alt_argv[0] != NULL)
    {
      if (last_directory != NULL)
        cwd = g_file_peek_path (last_directory);
    }

  if (cwd == NULL)
    {
      switch (ptyxis_profile_get_preserve_directory (profile))
        {
        case PTYXIS_PRESERVE_DIRECTORY_NEVER:
          break;

        case PTYXIS_PRESERVE_DIRECTORY_SAFE:
          if (!preserve_directory_safe)
            break;
          G_GNUC_FALLTHROUGH;

        case PTYXIS_PRESERVE_DIRECTORY_ALWAYS:
          cwd = maybe_extract_path_from_uri (last_directory, last_working_directory_uri);
          break;

        default:
          g_assert_not_reached ();
        }
    }

  if (cwd == NULL)
    cwd = "";

  full_argv = g_strv_builder_end (argv_builder);

  fd_list = g_unix_fd_list_new ();

  if (-1 == (handle = g_unix_fd_list_append (fd_list, pty_fd, &error)))
    {
      goto failure;
    }

  fd_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{uh}"));
  g_variant_builder_add (fd_builder, "{uh}", 0, handle);
  g_variant_builder_add (fd_builder, "{uh}", 1, handle);
  g_variant_builder_add (fd_builder, "{uh}", 2, handle);

  env_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{ss}"));
  for (guint i = 0; env[i]; i++)
    {
      const char *pair = env[i];
      const char *eq = strchr (pair, '=');
      const char *val = eq ? eq + 1 : "";
      g_autofree char *key = eq ? g_strndup (pair, eq - pair) : g_strdup (pair);

      g_variant_builder_add (env_builder, "{ss}", key, val);
    }

  if (g_cancellable_set_error_if_cancelled (dex_promise_get_cancellable (spawn->promise), &error))
    goto failure;

  self->spawning++;
  result = dex_await_boxed (ptyxis_ipc_container_call_spawn_future (container, cwd,
                                                                    (const char * const *)full_argv,
                                                                    g_variant_builder_end (fd_builder),
                                                                    g_variant_builder_end (env_builder), fd_list),
                           &error);
  if (result != NULL)
    proxy = dex_await_object (ptyxis_ipc_process_proxy_new_future (self->bus,
                                                                   G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                   NULL, result->process), &error);
  if (proxy != NULL)
    {
      gpointer status;

      process = _ptyxis_process_new (proxy);
      if (g_hash_table_lookup_extended (self->early_exits, result->process, NULL, &status))
        {
          _ptyxis_process_exited (process, GPOINTER_TO_INT (status));
          g_hash_table_remove (self->early_exits, result->process);
        }
      else if (g_dbus_connection_is_closed (self->bus))
        {
          _ptyxis_process_closed (process);
        }
      else
        {
          g_hash_table_insert (self->processes, g_strdup (result->process), _ptyxis_process_wait (process));
        }
    }
  else if (result != NULL && !g_dbus_connection_is_closed (self->bus))
    {
      dex_future_disown (dex_dbus_connection_call (self->bus,
                                                   NULL,
                                                   result->process,
                                                   "org.gnome.Ptyxis.Process",
                                                   "SendSignal",
                                                   g_variant_new ("(i)", SIGKILL),
                                                   NULL,
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1));
    }

  if (--self->spawning == 0)
    g_hash_table_remove_all (self->early_exits);

  if (process == NULL)
    goto failure;

  if (g_cancellable_is_cancelled (dex_promise_get_cancellable (spawn->promise)))
    _ptyxis_process_terminate (process);

  dex_promise_resolve_object (spawn->promise, g_steal_pointer (&process));

  return dex_future_new_true ();

failure:
  dex_promise_reject (spawn->promise, g_steal_pointer (&error));
  return dex_future_new_true ();
}

/**
 * ptyxis_client_spawn:
 * @self: a connected client
 * @container: the destination container
 * @profile: the terminal profile
 * @tab_uuid: (nullable): the terminal identifier
 * @directory: (nullable): the previous working directory URI
 * @pty: the consumer PTY
 * @argv: (nullable) (array zero-terminated=1): a command overriding the profile
 * @env: (nullable) (array zero-terminated=1): environment overrides
 *
 * Submitted requests remain tracked until the agent replies. If the caller
 * cancels, any resulting process is registered and terminated.
 *
 * Returns: (transfer full): a future resolving to a #PtyxisProcess
 */
DexFuture *
ptyxis_client_spawn (PtyxisClient       *self,
                     PtyxisIpcContainer *container,
                     PtyxisProfile      *profile,
                     const char         *tab_uuid,
                     const char         *directory,
                     VtePty             *pty,
                     const char * const *argv,
                     const char * const *env)
{
  Spawn *spawn;
  DexFuture *future;

  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);
  g_return_val_if_fail (PTYXIS_IPC_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (PTYXIS_IS_PROFILE (profile), NULL);
  g_return_val_if_fail (VTE_IS_PTY (pty), NULL);

  spawn = spawn_new ();
  spawn->client = g_object_ref (self);
  spawn->container = g_object_ref (container);
  spawn->profile = g_object_ref (profile);
  spawn->pty = g_object_ref (pty);
  spawn->tab_uuid = g_strdup (tab_uuid);
  spawn->directory = g_strdup (directory);
  spawn->argv = g_strdupv ((char **)argv);
  spawn->env = g_strdupv ((char **)env);
  spawn->promise = dex_promise_new_cancellable ();
  future = dex_ref (spawn->promise);

  dex_future_disown (dex_scheduler_spawn (NULL,
                                          0,
                                          ptyxis_client_spawn_fiber,
                                          spawn,
                                          (GDestroyNotify)spawn_free));

  return future;
}

const char *
ptyxis_client_get_os_name (PtyxisClient *self)
{
  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);

  return ptyxis_ipc_agent_get_os_name (self->proxy);
}

const char *
ptyxis_client_get_user_data_dir (PtyxisClient *self)
{
  g_return_val_if_fail (PTYXIS_IS_CLIENT (self), NULL);

  return ptyxis_ipc_agent_get_user_data_dir (self->proxy);
}
