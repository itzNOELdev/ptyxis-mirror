/* ptyxis-close-dialog.c
 *
 * Copyright 2021 Christian Hergert <christian@sourceandstack.com>
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

#include <glib/gi18n.h>

#include "ptyxis-tab.h"
#include "ptyxis-close-dialog.h"
#include "ptyxis-util.h"
#include "ptyxis-window.h"

static AdwDialog *
_ptyxis_close_dialog_new (GtkWindow *parent,
                          GPtrArray *tabs)
{
  const char *discard_label;
  PangoAttrList *smaller;
  AdwDialog *dialog;
  GtkWidget *group;

  g_return_val_if_fail (!parent || GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (tabs != NULL, NULL);
  g_return_val_if_fail (tabs->len > 0, NULL);
  g_return_val_if_fail (PTYXIS_IS_TAB (g_ptr_array_index (tabs, 0)), NULL);

  if (tabs->len == 1)
    ptyxis_tab_raise (g_ptr_array_index (tabs, 0));

  discard_label = g_dngettext (GETTEXT_PACKAGE, _("_Close"), _("_Close All"), tabs->len);

  dialog = adw_alert_dialog_new (_("Close Window?"),
                                   _("Some processes are still running."));

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                    "cancel", _("_Cancel"),
                                    "discard", discard_label,
                                    NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                              "discard", ADW_RESPONSE_DESTRUCTIVE);

  group = adw_preferences_group_new ();
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), group);

  smaller = pango_attr_list_new ();
  pango_attr_list_insert (smaller, pango_attr_scale_new (0.8333));

  for (guint i = 0; i < tabs->len; i++)
    {
      PtyxisTab *tab = g_ptr_array_index (tabs, i);
      g_autofree char *cmdline = NULL;
      g_autofree char *title = NULL;
      g_autofree char *subtitle = NULL;
      GtkWidget *row;
      GPid pid;

      if (ptyxis_tab_has_foreground_process (tab, &pid, &cmdline))
        {
          title = g_utf8_make_valid (cmdline ? cmdline : "", -1);
          subtitle = g_strdup_printf (_("Process %d"), pid);
        }
      else
        {
          title = ptyxis_tab_dup_title (tab);
          subtitle = ptyxis_tab_dup_subtitle (tab);
        }

      if (g_utf8_strlen (title, -1) > 200)
        g_utf8_offset_to_pointer (title, 200)[0] = 0;

      row = adw_action_row_new ();

      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);
      adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), row);


    }

  pango_attr_list_unref (smaller);

  return dialog;
}

typedef struct
{
  GWeakRef    dialog;
  GWeakRef    parent;
  DexPromise *promise;
  gulong      response_handler;
  gulong      destroy_handler;
  gboolean    parent_destroyed;
  gboolean    responded;
} CloseDialog;

static void
close_dialog_free (gpointer data)
{
  CloseDialog *close = data;
  g_autoptr(AdwDialog) dialog = g_weak_ref_get (&close->dialog);
  g_autoptr(GtkWindow) parent = g_weak_ref_get (&close->parent);

  if (parent != NULL)
    g_clear_signal_handler (&close->destroy_handler, parent);

  if (dialog != NULL)
    {
      g_clear_signal_handler (&close->response_handler, dialog);

      if (!close->responded &&
          !close->parent_destroyed &&
          gtk_widget_get_root (GTK_WIDGET (dialog)) != NULL)
        adw_dialog_force_close (dialog);
    }

  g_weak_ref_clear (&close->dialog);
  g_weak_ref_clear (&close->parent);
  dex_clear (&close->promise);
  g_free (close);
}

static void
ptyxis_close_dialog_response (AdwAlertDialog *dialog,
                              const char     *response,
                              CloseDialog    *close)
{
  g_assert (ADW_IS_ALERT_DIALOG (dialog));
  g_assert (close != NULL);

  close->responded = TRUE;

  if (dex_future_is_pending (DEX_FUTURE (close->promise)))
    dex_promise_resolve_boolean (close->promise,
                                 g_strcmp0 (response, "discard") == 0);
}

static void
ptyxis_close_dialog_parent_destroyed (GtkWindow   *parent,
                                      CloseDialog *close)
{
  close->parent_destroyed = TRUE;
  if (dex_future_is_pending (DEX_FUTURE (close->promise)))
    dex_promise_resolve_boolean (close->promise, FALSE);
}

static DexFuture *
ptyxis_close_dialog_completed (DexFuture *future,
                               gpointer   data)
{
  return NULL;
}

DexFuture *
_ptyxis_close_dialog_run (GtkWindow *parent,
                          GPtrArray *tabs)
{
  CloseDialog *close;
  AdwDialog *dialog;
  DexFuture *future;

  g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (tabs != NULL, NULL);

  if (tabs->len == 0)
    return dex_future_new_true ();

  dialog = _ptyxis_close_dialog_new (parent, tabs);

  close = g_new0 (CloseDialog, 1);
  close->promise = dex_promise_new ();
  g_weak_ref_init (&close->dialog, dialog);
  g_weak_ref_init (&close->parent, parent);

  close->response_handler = g_signal_connect (dialog,
                                              "response",
                                              G_CALLBACK (ptyxis_close_dialog_response),
                                              close);
  close->destroy_handler = g_signal_connect (parent,
                                             "destroy",
                                             G_CALLBACK (ptyxis_close_dialog_parent_destroyed),
                                             close);

  future = dex_future_finally (dex_ref (close->promise),
                               ptyxis_close_dialog_completed,
                               close,
                               close_dialog_free);

  adw_dialog_present (dialog, GTK_WIDGET (parent));

  return future;
}
