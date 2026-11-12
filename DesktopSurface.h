/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#pragma once

#include <wayland-client.h>

#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "DoubleBufferedSurface.h"

struct DesktopSurface
{
    struct DoubleBufferedSurface surface;

    struct wp_viewport *viewport;
    struct zwlr_layer_surface_v1 *zwlr_layer_surface;

    bool closed;
};

extern void DesktopSurface_Init(struct DesktopSurface *desktop_surface, struct wl_output *wl_output);
extern void DesktopSurface_Cleanup(struct DesktopSurface *desktop_surface);

extern const struct wl_surface_listener DesktopSurface_surface_listener;
extern const struct zwlr_layer_surface_v1_listener DesktopSurface_layer_surface_listener;
