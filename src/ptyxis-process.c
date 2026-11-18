/* ptyxis-process.c
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

#include <signal.h>

#include "ptyxis-process-private.h"

struct _PtyxisProcess
{
  GObject           parent_instance;
  PtyxisIpcProcess *proxy;
  DexPromise       *exited;
  gboolean          terminating;
};

G_DEFINE_FINAL_TYPE (PtyxisProcess, ptyxis_process, G_TYPE_OBJECT)

static void
ptyxis_process_finalize (GObject *object)
{
  PtyxisProcess *self = PTYXIS_PROCESS (object);

  if (self->proxy != NULL)
    _ptyxis_process_terminate (self);

  g_clear_object (&self->proxy);
  dex_clear (&self->exited);

  G_OBJECT_CLASS (ptyxis_process_parent_class)->finalize (object);
}

static void
ptyxis_process_class_init (PtyxisProcessClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = ptyxis_process_finalize;
}

static void
ptyxis_process_init (PtyxisProcess *self)
{
  self->exited = dex_promise_new ();
}

PtyxisProcess *
_ptyxis_process_new (PtyxisIpcProcess *proxy)
{
  PtyxisProcess *self;

  g_return_val_if_fail (PTYXIS_IPC_IS_PROCESS (proxy), NULL);

  self = g_object_new (PTYXIS_TYPE_PROCESS, NULL);
  self->proxy = g_object_ref (proxy);

  return self;
}

PtyxisIpcProcess *
_ptyxis_process_get_proxy (PtyxisProcess *self)
{
  g_return_val_if_fail (PTYXIS_IS_PROCESS (self), NULL);

  return self->proxy;
}

/**
 * _ptyxis_process_wait:
 * @self: a process
 *
 * All waiters share the same completion, including waiters arriving after exit.
 *
 * Returns: (transfer full): a future resolving to the raw wait status
 */
DexFuture *
_ptyxis_process_wait (PtyxisProcess *self)
{
  g_return_val_if_fail (PTYXIS_IS_PROCESS (self), NULL);

  return dex_ref (self->exited);
}

void
_ptyxis_process_exited (PtyxisProcess *self,
                        int            status)
{
  g_return_if_fail (PTYXIS_IS_PROCESS (self));

  if (dex_future_is_pending (DEX_FUTURE (self->exited)))
    dex_promise_resolve_int (self->exited, status);
}

void
_ptyxis_process_closed (PtyxisProcess *self)
{
  g_return_if_fail (PTYXIS_IS_PROCESS (self));

  if (dex_future_is_pending (DEX_FUTURE (self->exited)))
    dex_promise_reject (self->exited,
                        g_error_new_literal (G_IO_ERROR,
                                             G_IO_ERROR_CLOSED,
                                             "The connection to the agent has closed"));
}

DEX_DEFINE_CLOSURE_TYPE (ProcessTermination, process_termination,
                         DEX_DEFINE_CLOSURE_OBJECT (PtyxisIpcProcess, proxy),
                         DEX_DEFINE_CLOSURE_POINTER (DexFuture *, exited, dex_unref))

static DexFuture *
ptyxis_process_terminate_fiber (gpointer data)
{
  ProcessTermination *termination = data;
  g_autoptr(GError) error = NULL;

  if (!dex_future_is_pending (termination->exited))
    return dex_future_new_true ();

  dex_future_disown (ptyxis_ipc_process_call_send_signal_future (termination->proxy, SIGHUP));
  dex_await (dex_future_with_timeout_msec (dex_ref (termination->exited), 50), &error);

  if (g_error_matches (error, DEX_ERROR, DEX_ERROR_TIMED_OUT) &&
      dex_future_is_pending (termination->exited))
    dex_await (ptyxis_ipc_process_call_send_signal_future (termination->proxy, SIGKILL), NULL);

  return dex_future_new_true ();
}

void
_ptyxis_process_terminate (PtyxisProcess *self)
{
  g_return_if_fail (PTYXIS_IS_PROCESS (self));

  if (!self->terminating && dex_future_is_pending (DEX_FUTURE (self->exited)))
    {
      ProcessTermination *termination = process_termination_new ();

      self->terminating = TRUE;
      termination->proxy = g_object_ref (self->proxy);
      termination->exited = dex_ref (self->exited);

      dex_future_disown (dex_scheduler_spawn (NULL,
                                              0,
                                              ptyxis_process_terminate_fiber,
                                              termination,
                                              (GDestroyNotify)process_termination_free));
    }
}
