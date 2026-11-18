/* ptyxis-process-private.h
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

#pragma once

#include "ptyxis-application-ipc.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_PROCESS (ptyxis_process_get_type())

G_DECLARE_FINAL_TYPE (PtyxisProcess, ptyxis_process, PTYXIS, PROCESS, GObject)

void              _ptyxis_process_closed    (PtyxisProcess    *self);
void              _ptyxis_process_exited    (PtyxisProcess    *self,
                                             int               status);
PtyxisIpcProcess *_ptyxis_process_get_proxy (PtyxisProcess    *self);
PtyxisProcess    *_ptyxis_process_new       (PtyxisIpcProcess *proxy);
void              _ptyxis_process_terminate (PtyxisProcess    *self);
DexFuture        *_ptyxis_process_wait      (PtyxisProcess    *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
