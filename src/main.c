/* main.c
 *
 * Copyright 2023-2026 Christian Hergert <christian@sourceandstack.com>
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

#include <libdex.h>

#include <errno.h>
#include <sys/resource.h>

#include <glib/gi18n.h>

#include "ptyxis-application.h"
#include "ptyxis-early-options-private.h"
#include "ptyxis-palette.h"
#include "ptyxis-util.h"

static gint64 default_rlimit_nofile;

static gboolean
check_early_opts (int        *argc,
                  char     ***argv,
                  gboolean   *standalone)
{
  g_auto(PtyxisEarlyOptions) options = { 0 };
  g_autoptr(GError) error = NULL;

  if (!_ptyxis_early_options_parse (argc, argv, &options, &error))
    {
      g_printerr ("%s\n", error->message);
      return FALSE;
    }

  *standalone = options.standalone;

  if (options.version)
    {
      g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
      g_print ("\n");
      g_print ("  GTK: %d.%d.%d (Compiled against %d.%d.%d)\n",
               gtk_get_major_version (),
               gtk_get_minor_version (),
               gtk_get_micro_version (),
               GTK_MAJOR_VERSION,
               GTK_MINOR_VERSION,
               GTK_MICRO_VERSION);
      g_print ("  VTE: %d.%d.%d (Compiled against %d.%d.%d)\n",
               vte_get_major_version (),
               vte_get_minor_version (),
               vte_get_micro_version (),
               VTE_MAJOR_VERSION,
               VTE_MINOR_VERSION,
               VTE_MICRO_VERSION);
      g_print ("\n");
      g_print ("Copyright 2020-2026 Christian Hergert, et al.\n");
      g_print ("This is free software; see the source for copying conditions. There is NO\n");
      g_print ("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
      g_print ("\n");
      exit (EXIT_SUCCESS);
    }

  if (options.palette != NULL)
    {
      g_autoptr(DexFuture) future = ptyxis_palette_import (options.palette);

      while (dex_future_is_pending (future))
        g_main_context_iteration (NULL, TRUE);

      exit (dex_await_boolean (g_steal_pointer (&future), NULL) ? EXIT_SUCCESS : EXIT_FAILURE);
    }

  return TRUE;
}

static gint64
get_current_fd_limit (void)
{
  struct rlimit limit;

  if (getrlimit (RLIMIT_NOFILE, &limit) == 0)
    return limit.rlim_cur;

  return 0;
}

static void
bump_to_max_fd_limit (void)
{
  struct rlimit limit;

  if (getrlimit (RLIMIT_NOFILE, &limit) == 0)
    {
      limit.rlim_cur = limit.rlim_max;

      if (setrlimit (RLIMIT_NOFILE, &limit) != 0)
        g_warning ("Failed to set FD limit to %"G_GSSIZE_FORMAT"",
                    (gssize)limit.rlim_max);
      else
        g_debug ("Set RLIMIT_NOFILE to %"G_GSSIZE_FORMAT"",
                 (gssize)limit.rlim_max);
    }
}

gint64
ptyxis_application_get_default_rlimit_nofile (void)
{
  return default_rlimit_nofile;
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(PtyxisApplication) app = NULL;
  GApplicationFlags flags = G_APPLICATION_HANDLES_COMMAND_LINE | G_APPLICATION_HANDLES_OPEN;
  gboolean standalone = FALSE;
  int ret;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  g_set_prgname ("ptyxis");
  g_set_application_name (ptyxis_app_name ());

  dex_init ();
  if (!check_early_opts (&argc, &argv, &standalone))
    return EXIT_FAILURE;

  if (standalone)
    flags |= G_APPLICATION_NON_UNIQUE | G_APPLICATION_CAN_OVERRIDE_APP_ID;

  default_rlimit_nofile = get_current_fd_limit ();
  bump_to_max_fd_limit ();

  gtk_init ();

  app = ptyxis_application_new (APP_ID, flags);
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  return ret;
}
