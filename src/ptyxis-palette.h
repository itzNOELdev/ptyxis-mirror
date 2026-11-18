/*
 * ptyxis-palette.h
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

#include <libdex.h>

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_PALETTE (ptyxis_palette_get_type())

typedef struct _PtyxisPaletteScarf
{
  GdkRGBA foreground;
  GdkRGBA background;
} PtyxisPaletteScarf;

#define PTYXIS_PALETTE_SCARF_VISUAL_BELL 0
#define PTYXIS_PALETTE_SCARF_SUPERUSER   1
#define PTYXIS_PALETTE_SCARF_REMOTE      2
#define PTYXIS_PALETTE_N_SCARVES         3

typedef struct _PtyxisPaletteFace
{
  GdkRGBA background;
  GdkRGBA foreground;
  GdkRGBA titlebar_background;
  GdkRGBA titlebar_foreground;
  GdkRGBA selected_tab_background;
  GdkRGBA selected_tab_hover_background;
  GdkRGBA selected_tab_active_background;
  GdkRGBA cursor_bg;
  GdkRGBA cursor_fg;
  GdkRGBA indexed[16];
  union {
    PtyxisPaletteScarf scarves[PTYXIS_PALETTE_N_SCARVES];
    struct {
      PtyxisPaletteScarf visual_bell;
      PtyxisPaletteScarf superuser;
      PtyxisPaletteScarf remote;
    };
  };
  guint has_selected_tab_background : 1;
  guint has_selected_tab_hover_background : 1;
  guint has_selected_tab_active_background : 1;
} PtyxisPaletteFace;

G_DECLARE_FINAL_TYPE (PtyxisPalette, ptyxis_palette, PTYXIS, PALETTE, GObject)

char                    *ptyxis_get_user_palettes_dir          (void);
GListModel              *ptyxis_palette_get_all                (void);
const PtyxisPaletteFace *ptyxis_palette_get_face               (PtyxisPalette  *self,
                                                                gboolean        dark);
const char              *ptyxis_palette_get_id                 (PtyxisPalette  *self);
const char              *ptyxis_palette_get_name               (PtyxisPalette  *self);
gboolean                 ptyxis_palette_has_dark               (PtyxisPalette  *self);
gboolean                 ptyxis_palette_has_light              (PtyxisPalette  *self);
DexFuture               *ptyxis_palette_import                 (const char     *path) G_GNUC_WARN_UNUSED_RESULT;
void                     ptyxis_palette_init_user_palettes     (void);
gboolean                 ptyxis_palette_is_primary             (PtyxisPalette  *self);
GListModel              *ptyxis_palette_list_model_get_default (void);
DexFuture               *ptyxis_palette_load_user_palettes     (void) G_GNUC_WARN_UNUSED_RESULT;
PtyxisPalette           *ptyxis_palette_lookup                 (const char     *name);
PtyxisPalette           *ptyxis_palette_new_from_bytes         (const char     *file,
                                                                GBytes         *bytes,
                                                                GError        **error);
PtyxisPalette           *ptyxis_palette_new_from_resource      (const char     *file,
                                                                GError        **error);
gboolean                 ptyxis_palette_use_system_accent      (PtyxisPalette  *self);

G_END_DECLS
