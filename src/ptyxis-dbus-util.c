/* ptyxis-dbus-util.c
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

#include "ptyxis-dbus-util.h"

DexFuture *
ptyxis_dbus_connection_new_server (GIOStream  *stream,
                                   const char *guid,
                                   guint       timeout_seconds)
{
  dex_return_error_if_fail (G_IS_IO_STREAM (stream));
  dex_return_error_if_fail (guid != NULL);
  dex_return_error_if_fail (timeout_seconds > 0);

  return dex_future_with_timeout_seconds (
    dex_async_initable_new (G_TYPE_DBUS_CONNECTION,
                            G_PRIORITY_DEFAULT,
                            "stream", stream,
                            "guid", guid,
                            "flags", (G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS |
                                      G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER),
                            NULL),
    timeout_seconds);
}
