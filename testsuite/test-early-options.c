/* test-early-options.c
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

#include "ptyxis-early-options-private.h"

static void
test_option_values (void)
{
  const char *args[] = { "ptyxis", "--title", "--tab", "-x", "printf --new-window", NULL };
  g_auto(GStrv) argv = g_strdupv ((char **)args);
  PtyxisEarlyOptions options = { 0 };
  char **argv_ptr = argv;
  int argc = g_strv_length (argv);

  g_assert_true (_ptyxis_early_options_parse (&argc, &argv_ptr, &options, NULL));
  g_assert_true (options.standalone);
  g_assert_false (options.version);
  g_assert_cmpint (argc, ==, 5);

  _ptyxis_early_options_clear (&options);
}

static void
test_shared_command (void)
{
  const char *args[] = { "ptyxis", "--tab", "--", "printf", "%s", "--new-window", NULL };
  g_auto(GStrv) argv = g_strdupv ((char **)args);
  PtyxisEarlyOptions options = { 0 };
  char **argv_ptr = argv;
  int argc = g_strv_length (argv);

  g_assert_true (_ptyxis_early_options_parse (&argc, &argv_ptr, &options, NULL));
  g_assert_false (options.standalone);
  g_assert_cmpint (argc, ==, 4);
  g_assert_cmpstr (argv[2], ==, "-x");
  g_assert_cmpstr (argv[3], ==, "'printf' '%s' '--new-window'");

  _ptyxis_early_options_clear (&options);
}

static void
test_command_arguments (void)
{
  const char *args[] = { "ptyxis", "-x", "--tab", NULL };
  g_auto(GStrv) argv = g_strdupv ((char **)args);
  PtyxisEarlyOptions options = { 0 };
  char **argv_ptr = argv;
  int argc = g_strv_length (argv);

  g_assert_true (_ptyxis_early_options_parse (&argc, &argv_ptr, &options, NULL));
  g_assert_true (options.standalone);

  _ptyxis_early_options_clear (&options);
}

static void
test_grouped_short_options (void)
{
  const char *args[] = { "ptyxis", "-sx", "printf --tab", NULL };
  g_auto(GStrv) argv = g_strdupv ((char **)args);
  PtyxisEarlyOptions options = { 0 };
  char **argv_ptr = argv;
  int argc = g_strv_length (argv);

  g_assert_true (_ptyxis_early_options_parse (&argc, &argv_ptr, &options, NULL));
  g_assert_true (options.standalone);

  _ptyxis_early_options_clear (&options);
}

static void
test_legacy_execute (void)
{
  const char *args[] = { "ptyxis", "-e", "echo", "--tab", NULL };
  g_auto(GStrv) argv = g_strdupv ((char **)args);
  g_auto(PtyxisEarlyOptions) options = { 0 };
  char **argv_ptr = argv;
  int argc = g_strv_length (argv);

  g_assert_true (_ptyxis_early_options_parse (&argc, &argv_ptr, &options, NULL));
  g_assert_true (options.standalone);
  g_assert_cmpint (argc, ==, 3);
  g_assert_cmpstr (argv[1], ==, "-x");
  g_assert_cmpstr (argv[2], ==, "'echo' '--tab'");
}

static void
test_early_values (void)
{
  const char *args[] = { "ptyxis", "--import-palette", "--version", NULL };
  g_auto(GStrv) argv = g_strdupv ((char **)args);
  PtyxisEarlyOptions options = { 0 };
  char **argv_ptr = argv;
  int argc = g_strv_length (argv);

  g_assert_true (_ptyxis_early_options_parse (&argc, &argv_ptr, &options, NULL));
  g_assert_false (options.version);
  g_assert_cmpstr (options.palette, ==, "--version");

  _ptyxis_early_options_clear (&options);
}

static void
test_missing_command (void)
{
  const char *args[] = { "ptyxis", "-x", NULL };
  g_auto(GStrv) argv = g_strdupv ((char **)args);
  PtyxisEarlyOptions options = { 0 };
  g_autoptr(GError) error = NULL;
  char **argv_ptr = argv;
  int argc = g_strv_length (argv);

  g_assert_false (_ptyxis_early_options_parse (&argc, &argv_ptr, &options, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);

  _ptyxis_early_options_clear (&options);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/early-options/option-values", test_option_values);
  g_test_add_func ("/early-options/shared-command", test_shared_command);
  g_test_add_func ("/early-options/command-arguments", test_command_arguments);
  g_test_add_func ("/early-options/grouped-short-options", test_grouped_short_options);
  g_test_add_func ("/early-options/legacy-execute", test_legacy_execute);
  g_test_add_func ("/early-options/early-values", test_early_values);
  g_test_add_func ("/early-options/missing-command", test_missing_command);

  return g_test_run ();
}
