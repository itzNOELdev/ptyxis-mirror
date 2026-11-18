/* ptyxis-dbus-util.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "ptyxis-dbus-util.h"

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

typedef struct
{
  GWeakRef owner;
  PtyxisDBusPortalProxyCallback proxy_callback;
  PtyxisDBusPortalSettingsCallback settings_callback;
} PortalInit;

static void
portal_init_free (gpointer data)
{
  PortalInit *init = data;

  g_weak_ref_clear (&init->owner);
  g_free (init);
}

static DexFuture *
initialize_portal (gpointer data)
{
  PortalInit *init = data;
  g_autoptr(GObject) owner = NULL;
  const char *patterns[] = { "org.gnome.*", NULL };
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GVariant) all = NULL;

  if (!(connection = dex_await_object (dex_bus_get (G_BUS_TYPE_SESSION), NULL)))
    return dex_future_new_false ();

  proxy = dex_await_object (dex_async_initable_new (G_TYPE_DBUS_PROXY,
                                                    G_PRIORITY_DEFAULT,
                                                    "g-connection", connection,
                                                    "g-name", PORTAL_BUS_NAME,
                                                    "g-object-path", PORTAL_OBJECT_PATH,
                                                    "g-interface-name", PORTAL_SETTINGS_INTERFACE,
                                                    NULL),
                            NULL);
  owner = g_weak_ref_get (&init->owner);
  if (proxy == NULL || owner == NULL)
    return dex_future_new_false ();

  init->proxy_callback (owner, proxy);
  g_clear_object (&owner);

  all = dex_await_variant (dex_dbus_connection_call (connection, PORTAL_BUS_NAME,
                                                     PORTAL_OBJECT_PATH, PORTAL_SETTINGS_INTERFACE,
                                                     "ReadAll", g_variant_new ("(^as)", patterns),
                                                     G_VARIANT_TYPE ("(a{sa{sv}})"),
                                                     G_DBUS_CALL_FLAGS_NONE, -1), NULL);
  if ((owner = g_weak_ref_get (&init->owner)))
    init->settings_callback (owner, all);

  return dex_future_new_true ();
}

DexFuture *
ptyxis_dbus_initialize_portal (GObject                          *owner,
                               PtyxisDBusPortalProxyCallback     proxy_callback,
                               PtyxisDBusPortalSettingsCallback  settings_callback)
{
  PortalInit *init;

  dex_return_error_if_fail (G_IS_OBJECT (owner));
  dex_return_error_if_fail (proxy_callback != NULL);
  dex_return_error_if_fail (settings_callback != NULL);

  init = g_new0 (PortalInit, 1);
  g_weak_ref_init (&init->owner, owner);
  init->proxy_callback = proxy_callback;
  init->settings_callback = settings_callback;

  return dex_scheduler_spawn (NULL,
                              0,
                              initialize_portal,
                              init,
                              portal_init_free);
}

DexFuture *
ptyxis_dbus_connection_new_server (GIOStream  *stream,
                                   const char *guid,
                                   guint       timeout_seconds)
{
  dex_return_error_if_fail (G_IS_IO_STREAM (stream));
  dex_return_error_if_fail (guid != NULL);
  dex_return_error_if_fail (timeout_seconds > 0);

  return dex_future_with_timeout_seconds (dex_async_initable_new (G_TYPE_DBUS_CONNECTION,
                                                                  G_PRIORITY_DEFAULT,
                                                                  "stream", stream,
                                                                  "guid", guid,
                                                                  "flags", (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS |
                                                                            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER),
                                                                  NULL),
                                          timeout_seconds);
}
