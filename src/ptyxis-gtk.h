/* ptyxis-gtk.h
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

#include <gtk/gtk.h>
#include <libdex.h>

G_BEGIN_DECLS

static inline void
ptyxis_gdk_drop_read_value_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  value = gdk_drop_read_value_finish (GDK_DROP (object), result, &error);

  if (value != NULL)
    dex_promise_resolve (promise, value);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
ptyxis_gdk_drop_read_value (GdkDrop *drop,
                            GType    type,
                            int      io_priority)
{
  g_autoptr(DexPromise) promise = NULL;

  dex_return_error_if_fail (GDK_IS_DROP (drop));
  dex_return_error_if_fail (type != G_TYPE_INVALID);

  promise = dex_promise_new_cancellable ();

  gdk_drop_read_value_async (drop,
                             type,
                             io_priority,
                             dex_promise_get_cancellable (promise),
                             ptyxis_gdk_drop_read_value_cb,
                             dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

static inline void
ptyxis_gdk_drop_read_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;
  GInputStream *stream;

  stream = gdk_drop_read_finish (GDK_DROP (object), result, NULL, &error);

  if (stream != NULL)
    dex_promise_resolve_object (promise, stream);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
ptyxis_gdk_drop_read (GdkDrop     *drop,
                      const char **mime_types,
                      int          io_priority)
{
  g_autoptr(DexPromise) promise = NULL;

  dex_return_error_if_fail (GDK_IS_DROP (drop));
  dex_return_error_if_fail (mime_types != NULL && mime_types[0] != NULL);

  promise = dex_promise_new_cancellable ();

  gdk_drop_read_async (drop,
                       mime_types,
                       io_priority,
                       dex_promise_get_cancellable (promise),
                       ptyxis_gdk_drop_read_cb,
                       dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

static inline void
ptyxis_gtk_font_dialog_choose_font_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GError) error = NULL;
  PangoFontDescription *font;

  font = gtk_font_dialog_choose_font_finish (GTK_FONT_DIALOG (object), result, &error);

  if (font != NULL)
    dex_promise_resolve_boxed (promise, PANGO_TYPE_FONT_DESCRIPTION, font);
  else
    dex_promise_reject (promise, g_steal_pointer (&error));
}

static inline DexFuture *
ptyxis_gtk_font_dialog_choose_font (GtkFontDialog        *dialog,
                                    GtkWindow            *parent,
                                    PangoFontDescription *initial_value)
{
  g_autoptr(DexPromise) promise = NULL;

  dex_return_error_if_fail (GTK_IS_FONT_DIALOG (dialog));
  dex_return_error_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  promise = dex_promise_new_cancellable ();

  gtk_font_dialog_choose_font (dialog,
                               parent,
                               initial_value,
                               dex_promise_get_cancellable (promise),
                               ptyxis_gtk_font_dialog_choose_font_cb,
                               dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

G_END_DECLS
