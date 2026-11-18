/*
 * ptyxis-user-palettes.c
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

#include "config.h"

#include "ptyxis-user-palettes.h"

struct _PtyxisUserPalettes
{
  GObject       parent_instance;
  GFile        *directory;
  GFileMonitor *monitor;
  GHashTable   *file_to_palette;
  GHashTable   *id_to_palette;
  DexFuture    *loading;
  guint64       generation;
  GPtrArray    *items;
  guint         reload_source;
};

static gpointer
ptyxis_user_palettes_get_item (GListModel *model,
                               guint       position)
{
  PtyxisUserPalettes *self = PTYXIS_USER_PALETTES (model);

  if (position < self->items->len)
    return g_object_ref (g_ptr_array_index (self->items, position));

  return NULL;
}

static guint
ptyxis_user_palettes_get_n_items (GListModel *model)
{
  PtyxisUserPalettes *self = PTYXIS_USER_PALETTES (model);

  return self->items->len;
}

static GType
ptyxis_user_palettes_get_item_type (GListModel *model)
{
  return PTYXIS_TYPE_PALETTE;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ptyxis_user_palettes_get_n_items;
  iface->get_item = ptyxis_user_palettes_get_item;
  iface->get_item_type = ptyxis_user_palettes_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (PtyxisUserPalettes, ptyxis_user_palettes, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ptyxis_user_palettes_dispose (GObject *object)
{
  PtyxisUserPalettes *self = (PtyxisUserPalettes *)object;

  g_clear_handle_id (&self->reload_source, g_source_remove);

  if (self->monitor != NULL)
    g_file_monitor_cancel (self->monitor);

  g_clear_object (&self->directory);
  g_clear_object (&self->monitor);

  g_hash_table_remove_all (self->file_to_palette);
  g_hash_table_remove_all (self->id_to_palette);
  dex_clear (&self->loading);

  if (self->items->len > 0)
    g_ptr_array_remove_range (self->items, 0, self->items->len);

  G_OBJECT_CLASS (ptyxis_user_palettes_parent_class)->dispose (object);
}

static void
ptyxis_user_palettes_finalize (GObject *object)
{
  PtyxisUserPalettes *self = (PtyxisUserPalettes *)object;

  g_clear_pointer (&self->file_to_palette, g_hash_table_unref);
  g_clear_pointer (&self->id_to_palette, g_hash_table_unref);

  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (ptyxis_user_palettes_parent_class)->finalize (object);
}

static void
ptyxis_user_palettes_class_init (PtyxisUserPalettesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ptyxis_user_palettes_dispose;
  object_class->finalize = ptyxis_user_palettes_finalize;
}

static void
ptyxis_user_palettes_init (PtyxisUserPalettes *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
  self->file_to_palette = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 g_object_unref);
  self->id_to_palette = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               g_object_unref);

}

DEX_DEFINE_CLOSURE_TYPE (PaletteLoad, palette_load,
                         DEX_DEFINE_CLOSURE_VALUE_WITH_CLEAR (GWeakRef, palettes, g_weak_ref_clear),
                         DEX_DEFINE_CLOSURE_OBJECT (GFile, directory))

static void ptyxis_user_palettes_monitor_changed_cb (PtyxisUserPalettes *self,
                                                     GFile              *file,
                                                     GFile              *other_file,
                                                     GFileMonitorEvent   event_type,
                                                     GFileMonitor       *monitor);

static DexFuture *
ptyxis_user_palettes_load_fiber (gpointer data)
{
  PaletteLoad *load = data;

  dex_await (dex_file_make_directory_with_parents (load->directory), NULL);
  for (;;)
    {
      g_autoptr(PtyxisUserPalettes) self = g_weak_ref_get (&load->palettes);
      g_autoptr(GFileEnumerator) enumerator = NULL;
      g_autoptr(GError) enumeration_error = NULL;
      g_autoptr(GPtrArray) items = g_ptr_array_new_with_free_func (g_object_unref);
      g_autoptr(GHashTable) files = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           g_object_unref);
      g_autoptr(GHashTable) ids = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal,
                                                         g_free,
                                                         g_object_unref);
      guint64 generation;
      guint removed;

      if (self == NULL)
        return dex_future_new_false ();

      if (self->monitor == NULL)
        {
          self->monitor = g_file_monitor_directory (load->directory,
                                                    G_FILE_MONITOR_NONE,
                                                    NULL,
                                                    NULL);
          if (self->monitor != NULL)
            g_signal_connect_object (self->monitor,
                                     "changed",
                                     G_CALLBACK (ptyxis_user_palettes_monitor_changed_cb),
                                     self,
                                     G_CONNECT_SWAPPED);
        }

      generation = self->generation;

      g_clear_object (&self);
      enumerator = dex_await_object (dex_file_enumerate_children (load->directory,
                                                                  G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                                  G_FILE_QUERY_INFO_NONE,
                                                                  G_PRIORITY_LOW),
                                     NULL);

      if (enumerator == NULL)
        return dex_future_new_false ();

      for (;;)
        {
          g_autolist(GFileInfo) infos = dex_await_boxed (dex_file_enumerator_next_files (enumerator,
                                                                                         32,
                                                                                         G_PRIORITY_LOW),
                                                         &enumeration_error);

          if (infos == NULL)
            break;

          for (GList *iter = infos; iter != NULL; iter = iter->next)
            {
              g_autoptr(GFile) file = g_file_enumerator_get_child (enumerator, iter->data);
              g_autoptr(GBytes) bytes = NULL;
              g_autoptr(PtyxisPalette) palette = NULL;
              g_autoptr(GError) error = NULL;
              const char *path = g_file_peek_path (file);

              if (!g_str_has_suffix (path, ".palette"))
                continue;

              if ((bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &error)))
                palette = ptyxis_palette_new_from_bytes (path, bytes, &error);

              if (palette == NULL)
                {
                  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
                    g_warning ("Failed to load palette %s: %s", path, error->message);

                  if ((self = g_weak_ref_get (&load->palettes)))
                    {
                      if ((palette = g_hash_table_lookup (self->file_to_palette, path)))
                        g_object_ref (palette);
                    }

                  g_clear_object (&self);

                  if (palette == NULL)
                    continue;
                }

              g_hash_table_insert (files, g_strdup (path), g_object_ref (palette));
              g_hash_table_insert (ids,
                                   g_strdup (ptyxis_palette_get_id (palette)),
                                   g_object_ref (palette));
              g_ptr_array_add (items, g_steal_pointer (&palette));
            }
        }

      dex_await (dex_file_enumerator_close (enumerator, G_PRIORITY_LOW), NULL);
      if (enumeration_error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&enumeration_error));

      if (!(self = g_weak_ref_get (&load->palettes)))
        return dex_future_new_false ();

      if (generation != self->generation)
        continue;

      removed = self->items->len;
      g_ptr_array_unref (self->items);
      g_hash_table_unref (self->file_to_palette);
      g_hash_table_unref (self->id_to_palette);
      self->items = g_steal_pointer (&items);
      self->file_to_palette = g_steal_pointer (&files);
      self->id_to_palette = g_steal_pointer (&ids);

      g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, self->items->len);

      return dex_future_new_true ();
    }
}

/**
 * ptyxis_user_palettes_load:
 * @self: a palette collection
 *
 * Loads a complete snapshot and starts monitoring changes. The model is updated
 * on the main thread after all palette reads finish.
 *
 * Returns: (transfer full): the shared initial loading future
 */
DexFuture *
ptyxis_user_palettes_load (PtyxisUserPalettes *self)
{
  PaletteLoad *load;

  g_return_val_if_fail (PTYXIS_IS_USER_PALETTES (self), NULL);

  if (self->loading != NULL)
    return dex_ref (self->loading);

  load = palette_load_new ();
  g_weak_ref_init (&load->palettes, self);
  load->directory = g_object_ref (self->directory);
  self->loading = dex_scheduler_spawn (NULL,
                                       0,
                                       ptyxis_user_palettes_load_fiber,
                                       load,
                                       (GDestroyNotify)palette_load_free);

  return dex_ref (self->loading);
}

static gboolean
ptyxis_user_palettes_reload_cb (gpointer data)
{
  PtyxisUserPalettes *self = data;

  self->reload_source = 0;

  if (self->loading == NULL || !dex_future_is_pending (self->loading))
    {
      dex_clear (&self->loading);
      dex_future_disown (ptyxis_user_palettes_load (self));
    }

  return G_SOURCE_REMOVE;
}

static void
ptyxis_user_palettes_monitor_changed_cb (PtyxisUserPalettes *self,
                                         GFile              *file,
                                         GFile              *other_file,
                                         GFileMonitorEvent   event_type,
                                         GFileMonitor       *monitor)
{
  g_autofree char *name = g_file_get_basename (file);

  if (!g_str_has_suffix (name, ".palette"))
    return;

  self->generation++;

  if (self->reload_source == 0)
    self->reload_source = g_timeout_add_full (G_PRIORITY_LOW,
                                              100,
                                              ptyxis_user_palettes_reload_cb,
                                              self,
                                              NULL);
}

PtyxisUserPalettes *
ptyxis_user_palettes_new (const char *directory)
{
  PtyxisUserPalettes *self;

  g_return_val_if_fail (directory != NULL, NULL);

  self = g_object_new (PTYXIS_TYPE_USER_PALETTES, NULL);
  self->directory = g_file_new_for_path (directory);

  return self;
}

PtyxisPalette *
ptyxis_user_palettes_lookup (PtyxisUserPalettes *self,
                             const char         *id)
{
  PtyxisPalette *palette;

  g_return_val_if_fail (PTYXIS_IS_USER_PALETTES (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  palette = g_hash_table_lookup (self->id_to_palette, id);

  return palette ? g_object_ref (palette) : NULL;
}
