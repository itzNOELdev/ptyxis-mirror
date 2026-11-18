/* ptyxis-command-plan-private.h
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

typedef struct _PtyxisCommandPlan PtyxisCommandPlan;

typedef enum _PtyxisCommandAction
{
  PTYXIS_COMMAND_ACTION_ACTIVATE,
  PTYXIS_COMMAND_ACTION_PREFERENCES,
  PTYXIS_COMMAND_ACTION_TAB,
  PTYXIS_COMMAND_ACTION_WINDOW,
  PTYXIS_COMMAND_ACTION_COMMAND_WINDOW,
} PtyxisCommandAction;

PtyxisCommandPlan   *_ptyxis_command_plan_new                  (GVariantDict       *options,
                                                                const char         *caller_cwd,
                                                                GError            **error);
void                 _ptyxis_command_plan_execute              (PtyxisCommandPlan  *self,
                                                                PtyxisApplication  *application);
void                 _ptyxis_command_plan_free                 (PtyxisCommandPlan  *self);
PtyxisCommandAction  _ptyxis_command_plan_get_action           (PtyxisCommandPlan  *self);
const char          *_ptyxis_command_plan_get_cwd_uri          (PtyxisCommandPlan  *self);
const char * const  *_ptyxis_command_plan_get_argv             (PtyxisCommandPlan  *self);
gboolean             _ptyxis_command_plan_uses_restored_window (PtyxisCommandPlan  *self,
                                                                gboolean            did_restore,
                                                                gboolean            has_window);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PtyxisCommandPlan, _ptyxis_command_plan_free)

G_END_DECLS
