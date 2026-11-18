/* ptyxis-dbus-util.h
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

#include <libdex.h>

G_BEGIN_DECLS

typedef void (*PtyxisDBusPortalProxyCallback)    (gpointer    owner,
                                                  GDBusProxy *proxy);
typedef void (*PtyxisDBusPortalSettingsCallback) (gpointer    owner,
                                                  GVariant   *settings);

DexFuture *ptyxis_dbus_connection_new_server (GIOStream                        *stream,
                                              const char                       *guid,
                                              guint                             timeout_seconds) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *ptyxis_dbus_initialize_portal     (GObject                          *owner,
                                              PtyxisDBusPortalProxyCallback     proxy_callback,
                                              PtyxisDBusPortalSettingsCallback  settings_callback) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
