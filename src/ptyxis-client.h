/*
 * ptyxis-client.h
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

#pragma once

#include <gio/gio.h>
#include <vte/vte.h>

#include "ptyxis-application-ipc.h"
#include "ptyxis-profile.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_CLIENT (ptyxis_client_get_type())

G_DECLARE_FINAL_TYPE (PtyxisClient, ptyxis_client, PTYXIS, CLIENT, GObject)

DexFuture  *ptyxis_client_create_pty        (PtyxisClient       *self) G_GNUC_WARN_UNUSED_RESULT;
void        ptyxis_client_force_exit        (PtyxisClient       *self);
const char *ptyxis_client_get_os_name       (PtyxisClient       *self);
const char *ptyxis_client_get_user_data_dir (PtyxisClient       *self);
DexFuture  *ptyxis_client_new               (gboolean            use_sandbox) G_GNUC_WARN_UNUSED_RESULT;
DexFuture  *ptyxis_client_spawn             (PtyxisClient       *self,
                                             PtyxisIpcContainer *container,
                                             PtyxisProfile      *profile,
                                             const char         *tab_uuid,
                                             const char         *directory,
                                             VtePty             *pty,
                                             const char * const *argv,
                                             const char * const *env) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
