/* ptyxis-application-private.h
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

#include "ptyxis-application.h"

G_BEGIN_DECLS

typedef struct _PtyxisTerminal PtyxisTerminal;
typedef struct _PtyxisWindow   PtyxisWindow;

typedef void (*PtyxisApplicationRequestFunc) (PtyxisApplication *self,
                                              gpointer           data);

void          _ptyxis_application_queue_request           (PtyxisApplication            *self,
                                                           PtyxisApplicationRequestFunc  callback,
                                                           gpointer                      data,
                                                           GDestroyNotify                destroy);
void          _ptyxis_application_set_agent_path          (PtyxisApplication            *self,
                                                           const char                   *agent_path);
gboolean      _ptyxis_application_restore_session         (PtyxisApplication            *self);
void          _ptyxis_application_apply_default_size      (PtyxisApplication            *self,
                                                           PtyxisTerminal               *terminal);
void          _ptyxis_application_activate_window         (GApplication                 *app,
                                                           const char                   *title,
                                                           gboolean                      maximize,
                                                           gboolean                      fullscreen);
PtyxisWindow *_ptyxis_application_get_current_window      (PtyxisApplication            *self);
gboolean      _ptyxis_application_should_ignore_osc_title (PtyxisApplication            *self,
                                                           const char                   *title);
void          _ptyxis_application_apply_window_state      (PtyxisWindow                 *window,
                                                           gboolean                      maximize,
                                                           gboolean                      fullscreen);

G_END_DECLS
