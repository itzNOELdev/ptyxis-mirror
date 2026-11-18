/*
 * ptyxis-session.h
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

#include "ptyxis-application.h"

G_BEGIN_DECLS

#define PTYXIS_TYPE_SESSION (ptyxis_session_get_type())

G_DECLARE_FINAL_TYPE (PtyxisSession, ptyxis_session, PTYXIS, SESSION, GObject)

PtyxisSession *ptyxis_session_new        (void);
gboolean       ptyxis_session_has_state  (PtyxisSession     *self);
void           ptyxis_session_take_state (PtyxisSession     *self,
                                          GVariant          *state);
gboolean       ptyxis_session_restore    (PtyxisSession     *self,
                                          PtyxisApplication *application);
DexFuture     *ptyxis_session_load       (PtyxisSession     *self) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *ptyxis_session_save       (PtyxisSession     *self,
                                          PtyxisApplication *application) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *ptyxis_session_flush      (PtyxisSession     *self) G_GNUC_WARN_UNUSED_RESULT;
G_END_DECLS
