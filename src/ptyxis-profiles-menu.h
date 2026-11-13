/*
 * ptyxis-profiles-menu.h
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PTYXIS_TYPE_PROFILES_MENU (ptyxis_profiles_menu_get_type())

G_DECLARE_FINAL_TYPE (PtyxisProfilesMenu, ptyxis_profiles_menu, PTYXIS, PROFILES_MENU, GtkWidget)

void ptyxis_profiles_menu_set_narrow (PtyxisProfilesMenu *self,
                                      gboolean            narrow);

G_END_DECLS
