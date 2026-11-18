/* ptyxis-early-options-private.h
 *
 * Copyright 2026 Christian Hergert
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

G_BEGIN_DECLS

typedef struct _PtyxisEarlyOptions
{
  char     *palette;
  gboolean  standalone;
  gboolean  version;
} PtyxisEarlyOptions;

gboolean _ptyxis_early_options_parse (int                  *argc,
                                      char               ***argv,
                                      PtyxisEarlyOptions   *options,
                                      GError              **error);
void     _ptyxis_early_options_clear (PtyxisEarlyOptions   *options);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (PtyxisEarlyOptions, _ptyxis_early_options_clear)

G_END_DECLS
