/* ptyxis-early-options.c
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
#include <string.h>

#include "ptyxis-early-options-private.h"

static gboolean
option_has_value (const char *arg)
{
  return g_str_equal (arg, "--working-directory") ||
         g_str_equal (arg, "--title") ||
         g_str_equal (arg, "--tab-with-profile") ||
         g_str_equal (arg, "--gapplication-app-id") ||
         g_str_equal (arg, "--display") ||
         g_str_equal (arg, "--name") ||
         g_str_equal (arg, "--class") ||
         g_str_equal (arg, "--gtk-debug") ||
         g_str_equal (arg, "--gtk-no-debug");
}

gboolean
_ptyxis_early_options_parse (int                  *argc,
                             char               ***argv,
                             PtyxisEarlyOptions   *options,
                             GError              **error)
{
  gboolean shared_target = FALSE;
  gboolean command = FALSE;
  gboolean explicit_standalone = FALSE;

  g_return_val_if_fail (argc != NULL, FALSE);
  g_return_val_if_fail (argv != NULL && *argv != NULL, FALSE);
  g_return_val_if_fail (options != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  for (int i = 1; i < *argc; i++)
    {
      const char *arg = (*argv)[i];

      if (g_str_equal (arg, "-e"))
        {
          (*argv)[i][1] = '-';
          arg = (*argv)[i];
        }

      if (g_str_equal (arg, "--"))
        {
          if (i + 1 < *argc)
            {
              GString *quoted_command = g_string_new (NULL);

              for (int j = i + 1; j < *argc; j++)
                {
                  g_autofree char *quoted = g_shell_quote ((*argv)[j]);

                  if (j > i + 1)
                    g_string_append_c (quoted_command, ' ');
                  g_string_append (quoted_command, quoted);
                  (*argv)[j] = NULL;
                }

              (*argv)[i] = g_strdup ("-x");
              (*argv)[i + 1] = g_string_free (quoted_command, FALSE);
              *argc = i + 2;
              command = TRUE;
            }

          break;
        }

      if (arg[0] == '-' && arg[1] != '-' && arg[1] != 0)
        {
          for (int j = 1; arg[j] != 0; j++)
            {
              if (arg[j] == 's')
                explicit_standalone = TRUE;
              else if (arg[j] == 'x' || arg[j] == 'd' || arg[j] == 'T')
                {
                  if (arg[j] == 'x')
                    command = TRUE;

                  if (arg[j + 1] == 0 && ++i >= *argc)
                    {
                      g_set_error (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   _("%s requires an argument"),
                                   arg);
                      return FALSE;
                    }

                  break;
                }
            }

          continue;
        }

      if (g_str_equal (arg, "--tab") ||
          g_str_equal (arg, "--new-window") ||
          g_str_equal (arg, "--tab-with-profile") ||
          g_str_has_prefix (arg, "--tab-with-profile="))
        shared_target = TRUE;

      if (g_str_equal (arg, "--standalone"))
        explicit_standalone = TRUE;

      if (g_str_equal (arg, "--version"))
        options->version = TRUE;

      if (g_str_has_prefix (arg, "--import-palette="))
        g_set_str (&options->palette, arg + strlen ("--import-palette="));
      else if (g_str_equal (arg, "--import-palette"))
        {
          if (++i >= *argc)
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   _("--import-palette requires a filename"));
              return FALSE;
            }

          g_set_str (&options->palette, (*argv)[i]);
          continue;
        }

      if (g_str_equal (arg, "--execute"))
        {
          command = TRUE;

          if (++i >= *argc)
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   _("--execute requires a command"));
              return FALSE;
            }

          continue;
        }

      if (g_str_has_prefix (arg, "--execute="))
        command = TRUE;

      if (option_has_value (arg))
        {
          if (++i >= *argc)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("%s requires an argument"),
                           arg);
              return FALSE;
            }
        }
    }

  options->standalone = explicit_standalone || (command && !shared_target);

  return TRUE;
}

void
_ptyxis_early_options_clear (PtyxisEarlyOptions *options)
{
  g_return_if_fail (options != NULL);

  g_clear_pointer (&options->palette, g_free);
  options->standalone = FALSE;
  options->version = FALSE;
}
