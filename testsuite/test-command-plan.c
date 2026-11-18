/* test-command-plan.c
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

#include "ptyxis-command-plan-private.h"

gint64
ptyxis_application_get_default_rlimit_nofile (void)
{
  return 0;
}

static void
test_actions (void)
{
  g_autoptr(GVariantDict) options = g_variant_dict_new (NULL);
  g_autoptr(PtyxisCommandPlan) plan = NULL;

  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);
  g_assert_nonnull (plan);
  g_assert_cmpint (_ptyxis_command_plan_get_action (plan), ==, PTYXIS_COMMAND_ACTION_ACTIVATE);

  g_variant_dict_insert (options, "title", "s", "A title");
  g_clear_pointer (&plan, _ptyxis_command_plan_free);
  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);
  g_assert_cmpint (_ptyxis_command_plan_get_action (plan), ==, PTYXIS_COMMAND_ACTION_WINDOW);

  g_variant_dict_remove (options, "title");
  g_variant_dict_insert_value (options,
                               "working-directory",
                               g_variant_new_bytestring ("/tmp/another"));
  g_clear_pointer (&plan, _ptyxis_command_plan_free);
  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);
  g_assert_cmpint (_ptyxis_command_plan_get_action (plan), ==, PTYXIS_COMMAND_ACTION_WINDOW);

  g_variant_dict_insert (options, "tab", "b", TRUE);
  g_clear_pointer (&plan, _ptyxis_command_plan_free);
  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);
  g_assert_cmpint (_ptyxis_command_plan_get_action (plan), ==, PTYXIS_COMMAND_ACTION_TAB);

  g_clear_pointer (&options, g_variant_dict_unref);
  options = g_variant_dict_new (NULL);
  g_variant_dict_insert (options, "preferences", "b", TRUE);
  g_clear_pointer (&plan, _ptyxis_command_plan_free);
  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);
  g_assert_cmpint (_ptyxis_command_plan_get_action (plan), ==, PTYXIS_COMMAND_ACTION_PREFERENCES);
}

static void
test_command (void)
{
  g_autoptr(GVariantDict) options = g_variant_dict_new (NULL);
  g_autoptr(PtyxisCommandPlan) plan = NULL;
  const char * const *argv;

  g_variant_dict_insert (options, "execute", "s", "printf '%s %s' one two");
  g_variant_dict_insert_value (options,
                               "working-directory",
                               g_variant_new_bytestring ("sub dir"));

  plan = _ptyxis_command_plan_new (options, "/tmp/base", NULL);
  g_assert_nonnull (plan);
  g_assert_cmpint (_ptyxis_command_plan_get_action (plan), ==, PTYXIS_COMMAND_ACTION_COMMAND_WINDOW);
  g_assert_cmpstr (_ptyxis_command_plan_get_cwd_uri (plan), ==, "file:///tmp/base/sub%20dir");

  argv = _ptyxis_command_plan_get_argv (plan);
  g_assert_cmpstr (argv[0], ==, "printf");
  g_assert_cmpstr (argv[1], ==, "%s %s");
  g_assert_cmpstr (argv[2], ==, "one");
  g_assert_cmpstr (argv[3], ==, "two");
  g_assert_null (argv[4]);

  g_variant_dict_insert (options, "new-window", "b", TRUE);
  g_clear_pointer (&plan, _ptyxis_command_plan_free);
  plan = _ptyxis_command_plan_new (options, "/tmp/base", NULL);
  g_assert_cmpint (_ptyxis_command_plan_get_action (plan), ==, PTYXIS_COMMAND_ACTION_WINDOW);
}

static void
test_absolute_directory (void)
{
  g_autoptr(GVariantDict) options = g_variant_dict_new (NULL);
  g_autoptr(PtyxisCommandPlan) plan = NULL;

  g_variant_dict_insert_value (options,
                               "working-directory",
                               g_variant_new_bytestring ("/tmp/a # b"));
  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);

  g_assert_cmpstr (_ptyxis_command_plan_get_cwd_uri (plan), ==, "file:///tmp/a%20%23%20b");
}

static void
test_conflicts (void)
{
  g_autoptr(GVariantDict) options = g_variant_dict_new (NULL);
  g_autoptr(PtyxisCommandPlan) plan = NULL;
  g_autoptr(GError) error = NULL;

  g_variant_dict_insert (options, "tab-with-profile", "s", "uuid");
  g_variant_dict_insert (options, "new-window", "b", TRUE);
  plan = _ptyxis_command_plan_new (options, "/tmp", &error);
  g_assert_null (plan);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);

  g_clear_pointer (&options, g_variant_dict_unref);
  options = g_variant_dict_new (NULL);
  g_clear_error (&error);
  g_variant_dict_insert (options, "preferences", "b", TRUE);
  g_variant_dict_insert (options, "execute", "s", "true");
  plan = _ptyxis_command_plan_new (options, "/tmp", &error);
  g_assert_null (plan);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);

  g_clear_pointer (&options, g_variant_dict_unref);
  options = g_variant_dict_new (NULL);
  g_clear_error (&error);
  g_variant_dict_insert (options, "execute", "s", "'unclosed");
  plan = _ptyxis_command_plan_new (options, "/tmp", &error);
  g_assert_null (plan);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
}

static void
test_restored_window (void)
{
  g_autoptr(GVariantDict) options = g_variant_dict_new (NULL);
  g_autoptr(PtyxisCommandPlan) plan = NULL;

  g_variant_dict_insert (options, "new-window", "b", TRUE);
  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);

  g_assert_true (_ptyxis_command_plan_uses_restored_window (plan, TRUE, TRUE));
  g_assert_false (_ptyxis_command_plan_uses_restored_window (plan, FALSE, TRUE));
  g_assert_false (_ptyxis_command_plan_uses_restored_window (plan, TRUE, FALSE));

  g_variant_dict_insert (options, "title", "s", "New title");
  g_clear_pointer (&plan, _ptyxis_command_plan_free);
  plan = _ptyxis_command_plan_new (options, "/tmp", NULL);

  g_assert_false (_ptyxis_command_plan_uses_restored_window (plan, TRUE, TRUE));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/command-plan/actions", test_actions);
  g_test_add_func ("/command-plan/command", test_command);
  g_test_add_func ("/command-plan/absolute-directory", test_absolute_directory);
  g_test_add_func ("/command-plan/conflicts", test_conflicts);
  g_test_add_func ("/command-plan/restored-window", test_restored_window);

  return g_test_run ();
}
