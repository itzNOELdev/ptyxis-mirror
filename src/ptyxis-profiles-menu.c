/*
 * ptyxis-profiles-menu.c
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

#include "config.h"

#include "ptyxis-profiles-menu.h"

struct _PtyxisProfilesMenu
{
  GtkWidget  parent_instance;
  GtkWidget *child;
  guint      narrow : 1;
};

G_DEFINE_FINAL_TYPE (PtyxisProfilesMenu, ptyxis_profiles_menu, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_CHILD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
ptyxis_profiles_menu_set_child (PtyxisProfilesMenu *self,
                                GtkWidget          *child)
{
  g_assert (PTYXIS_IS_PROFILES_MENU (self));
  g_assert (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  if (self->child != NULL)
    gtk_widget_unparent (self->child);

  self->child = child;

  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

static void
ptyxis_profiles_menu_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  PtyxisProfilesMenu *self = PTYXIS_PROFILES_MENU (widget);

  if (self->child != NULL)
    gtk_widget_measure (self->child,
                        orientation,
                        for_size,
                        minimum,
                        natural,
                        minimum_baseline,
                        natural_baseline);

  if (orientation == GTK_ORIENTATION_VERTICAL && self->narrow && natural != NULL)
    {
      GtkRoot *root = gtk_widget_get_root (widget);

      if (root != NULL)
        *natural = MAX (*natural, gtk_widget_get_height (GTK_WIDGET (root)));
    }
}

static void
ptyxis_profiles_menu_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  PtyxisProfilesMenu *self = PTYXIS_PROFILES_MENU (widget);

  if (self->child != NULL)
    gtk_widget_allocate (self->child, width, height, baseline, NULL);
}

static void
ptyxis_profiles_menu_dispose (GObject *object)
{
  PtyxisProfilesMenu *self = PTYXIS_PROFILES_MENU (object);

  if (self->child != NULL)
    gtk_widget_unparent (self->child);
  self->child = NULL;

  G_OBJECT_CLASS (ptyxis_profiles_menu_parent_class)->dispose (object);
}

static void
ptyxis_profiles_menu_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  PtyxisProfilesMenu *self = PTYXIS_PROFILES_MENU (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_profiles_menu_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  PtyxisProfilesMenu *self = PTYXIS_PROFILES_MENU (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      ptyxis_profiles_menu_set_child (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ptyxis_profiles_menu_class_init (PtyxisProfilesMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ptyxis_profiles_menu_dispose;
  object_class->get_property = ptyxis_profiles_menu_get_property;
  object_class->set_property = ptyxis_profiles_menu_set_property;
  widget_class->measure = ptyxis_profiles_menu_measure;
  widget_class->size_allocate = ptyxis_profiles_menu_size_allocate;

  properties[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
  gtk_widget_class_set_css_name (widget_class, "profilesmenu");
}

static void
ptyxis_profiles_menu_init (PtyxisProfilesMenu *self)
{
}

void
ptyxis_profiles_menu_set_narrow (PtyxisProfilesMenu *self,
                                 gboolean            narrow)
{
  g_return_if_fail (PTYXIS_IS_PROFILES_MENU (self));

  narrow = !!narrow;

  if (self->narrow == narrow)
    return;

  self->narrow = narrow;
  gtk_widget_queue_resize (GTK_WIDGET (self));
}
