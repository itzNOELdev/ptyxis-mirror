/* ptyxis-command-plan.c
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

#include "config.h"

#include <glib/gi18n.h>

#include "ptyxis-application-private.h"
#include "ptyxis-command-plan-private.h"
#include "ptyxis-preferences-window.h"
#include "ptyxis-tab.h"
#include "ptyxis-terminal.h"
#include "ptyxis-util.h"
#include "ptyxis-window.h"

struct _PtyxisCommandPlan
{
  char                 *cwd_uri;
  char                 *profile_uuid;
  char                 *title;
  char                **argv;
  PtyxisCommandAction   action;
  gboolean              explicit_directory;
  gboolean              explicit_title;
  gboolean              requested_window;
  gboolean              maximize;
  gboolean              fullscreen;
  gboolean              conflicting_window_state;
  gboolean              executed;
};

static char *
resolve_directory (const char *directory,
                   const char *caller_cwd)
{
  g_autoptr(GFile) file = NULL;
  g_autofree char *path = NULL;

  if (directory == NULL)
    directory = caller_cwd;

  if (directory == NULL)
    return NULL;

  if (g_uri_peek_scheme (directory) != NULL)
    return g_strdup (directory);

  if (g_path_is_absolute (directory))
    {
      file = g_file_new_for_path (directory);
    }
  else
    {
      const char *base = ptyxis_str_empty0 (caller_cwd) ? g_get_home_dir () : caller_cwd;

      path = g_build_filename (base, directory, NULL);
      file = g_file_new_for_path (path);
    }

  return g_file_get_uri (file);
}

PtyxisCommandPlan *
_ptyxis_command_plan_new (GVariantDict  *options,
                          const char    *caller_cwd,
                          GError       **error)
{
  g_autoptr(PtyxisCommandPlan) self = NULL;
  g_autofree char *directory = NULL;
  g_autofree char *command = NULL;
  g_autoptr(GError) parse_error = NULL;
  gboolean preferences;
  gboolean new_tab;
  gboolean new_window;
  gboolean with_profile;
  gboolean has_command;
  guint selectors;

  g_return_val_if_fail (options != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  self = g_new0 (PtyxisCommandPlan, 1);
  preferences = g_variant_dict_contains (options, "preferences");
  new_tab = g_variant_dict_contains (options, "tab");
  new_window = g_variant_dict_contains (options, "new-window");
  with_profile = g_variant_dict_contains (options, "tab-with-profile");
  has_command = g_variant_dict_contains (options, "execute");
  selectors = (guint)new_tab + (guint)new_window + (guint)with_profile;

  self->requested_window = new_window;
  self->explicit_directory = g_variant_dict_contains (options, "working-directory");
  self->explicit_title = g_variant_dict_contains (options, "title");

  g_variant_dict_lookup (options, "maximize", "b", &self->maximize);
  g_variant_dict_lookup (options, "fullscreen", "b", &self->fullscreen);
  g_variant_dict_lookup (options, "title", "s", &self->title);
  g_variant_dict_lookup (options, "tab-with-profile", "s", &self->profile_uuid);
  g_variant_dict_lookup (options, "working-directory", "^ay", &directory);
  g_variant_dict_lookup (options, "execute", "s", &command);

  if (selectors > 1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("--tab, --tab-with-profile, or --new-window may not be used together"));
      return NULL;
    }

  if (preferences &&
      (selectors > 0 ||
       has_command ||
       self->explicit_directory ||
       self->explicit_title ||
       self->maximize ||
       self->fullscreen))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("--preferences may not be combined with terminal options"));
      return NULL;
    }

  if (has_command)
    {
      if (command == NULL || !g_shell_parse_argv (command, NULL, &self->argv, &parse_error))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Cannot parse command: %s"),
                       parse_error != NULL ? parse_error->message : "");
          return NULL;
        }
    }

  self->cwd_uri = resolve_directory (directory, caller_cwd);

  if (preferences)
    self->action = PTYXIS_COMMAND_ACTION_PREFERENCES;
  else if (new_tab || with_profile)
    self->action = PTYXIS_COMMAND_ACTION_TAB;
  else if (new_window ||
           (!has_command &&
            (self->explicit_directory ||
             self->explicit_title)))
    self->action = PTYXIS_COMMAND_ACTION_WINDOW;
  else if (has_command)
    self->action = PTYXIS_COMMAND_ACTION_COMMAND_WINDOW;
  else
    self->action = PTYXIS_COMMAND_ACTION_ACTIVATE;

  if (self->fullscreen && self->maximize)
    {
      self->conflicting_window_state = TRUE;
      self->maximize = FALSE;
    }

  return g_steal_pointer (&self);
}

void
_ptyxis_command_plan_free (PtyxisCommandPlan *self)
{
  if (self == NULL)
    return;

  g_clear_pointer (&self->cwd_uri, g_free);
  g_clear_pointer (&self->profile_uuid, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->argv, g_strfreev);
  g_free (self);
}

PtyxisCommandAction
_ptyxis_command_plan_get_action (PtyxisCommandPlan *self)
{
  g_return_val_if_fail (self != NULL, PTYXIS_COMMAND_ACTION_ACTIVATE);

  return self->action;
}

const char *
_ptyxis_command_plan_get_cwd_uri (PtyxisCommandPlan *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->cwd_uri;
}

const char * const *
_ptyxis_command_plan_get_argv (PtyxisCommandPlan *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return (const char * const *)self->argv;
}

gboolean
_ptyxis_command_plan_uses_restored_window (PtyxisCommandPlan *self,
                                           gboolean           did_restore,
                                           gboolean           has_window)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->action == PTYXIS_COMMAND_ACTION_WINDOW &&
         self->requested_window &&
         did_restore &&
         has_window &&
         !self->explicit_directory &&
         !self->explicit_title &&
         !self->maximize &&
         !self->fullscreen &&
         self->argv == NULL;
}

static void
add_terminal (PtyxisCommandPlan *self,
              PtyxisApplication *application,
              PtyxisWindow      *window,
              gboolean           created_window)
{
  g_autoptr(PtyxisProfile) profile = NULL;
  PtyxisTerminal *terminal;
  PtyxisTab *tab;

  g_assert (self != NULL);
  g_assert (PTYXIS_IS_APPLICATION (application));
  g_assert (PTYXIS_IS_WINDOW (window));

  if (self->profile_uuid != NULL)
    profile = ptyxis_application_dup_profile (application, self->profile_uuid);
  else
    profile = ptyxis_application_dup_default_profile (application);

  if (self->argv != NULL)
    tab = ptyxis_window_add_tab_for_command (window,
                                             profile,
                                             (const char * const *)self->argv,
                                             self->cwd_uri);
  else
    tab = ptyxis_tab_new (profile);

  terminal = ptyxis_tab_get_terminal (tab);

  if (created_window)
    _ptyxis_application_apply_default_size (application, terminal);

  ptyxis_tab_set_initial_working_directory_uri (tab, self->cwd_uri);
  ptyxis_tab_set_title_prefix (tab, self->title);
  ptyxis_tab_set_ignore_osc_title (tab, _ptyxis_application_should_ignore_osc_title (application, self->title));

  if (self->argv == NULL)
    ptyxis_window_add_tab (window, tab);

  ptyxis_window_set_active_tab (window, tab);
}

void
_ptyxis_command_plan_execute (PtyxisCommandPlan *self,
                              PtyxisApplication *application)
{
  PtyxisWindow *window;
  gboolean did_restore = FALSE;
  gboolean created_window = FALSE;

  g_return_if_fail (self != NULL);
  g_return_if_fail (PTYXIS_IS_APPLICATION (application));
  g_return_if_fail (!self->executed);

  self->executed = TRUE;

  if (self->conflicting_window_state)
    g_warning ("Both maximize and fullscreen specified, preferring fullscreen");

  if (!(g_application_get_flags (G_APPLICATION (application)) & G_APPLICATION_NON_UNIQUE))
    did_restore = _ptyxis_application_restore_session (application);

  if (self->action == PTYXIS_COMMAND_ACTION_PREFERENCES)
    {
      PtyxisWindow *parent = _ptyxis_application_get_current_window (application);

      ptyxis_preferences_window_present (GTK_WIDGET (parent),
                                         parent != NULL &&
                                         gtk_widget_get_width (GTK_WIDGET (parent)) <= 400);
      return;
    }

  if (self->action == PTYXIS_COMMAND_ACTION_ACTIVATE)
    {
      _ptyxis_application_activate_window (G_APPLICATION (application),
                                           self->title,
                                           self->maximize,
                                           self->fullscreen);
      return;
    }

  if (self->action == PTYXIS_COMMAND_ACTION_COMMAND_WINDOW)
    {
      PtyxisTab *tab;

      window = ptyxis_window_new_for_command (NULL,
                                              (const char * const *)self->argv,
                                              self->cwd_uri);
      tab = ptyxis_window_get_active_tab (window);
      ptyxis_tab_set_title_prefix (tab, self->title);
      ptyxis_tab_set_ignore_osc_title (tab, _ptyxis_application_should_ignore_osc_title (application, self->title));
      _ptyxis_application_apply_window_state (window, self->maximize, self->fullscreen);
      gtk_application_add_window (GTK_APPLICATION (application), GTK_WINDOW (window));
      gtk_window_present (GTK_WINDOW (window));
      return;
    }

  window = _ptyxis_application_get_current_window (application);

  if (self->action == PTYXIS_COMMAND_ACTION_WINDOW)
    {
      if (_ptyxis_command_plan_uses_restored_window (self, did_restore, window != NULL))
        {
          gtk_window_present (GTK_WINDOW (window));
          return;
        }

      window = NULL;
    }

  if (window == NULL)
    {
      window = ptyxis_window_new_empty ();
      created_window = TRUE;
    }

  add_terminal (self, application, window, created_window);

  if (created_window)
    _ptyxis_application_apply_window_state (window, self->maximize, self->fullscreen);

  gtk_window_present (GTK_WINDOW (window));
}
