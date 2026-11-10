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

struct DesktopLayerSurface
{
    struct DoubleBufferedSurface double_buffered_surface;

    struct wp_viewport *viewport;
    struct zwlr_layer_surface_v1 *zwlr_layer_surface;

    int w;
    int h;

    bool closed;
};

extern bool DesktopLayerSurface_Init(struct DesktopLayerSurface *layer_surface, struct wl_output *output);
extern void DesktopLayerSurface_Cleanup(struct DesktopLayerSurface *layer_surface);

extern struct wl_surface *DesktopLayerSurface_GetWlSurface(struct DesktopLayerSurface *layer_surface);

extern const struct wl_surface_listener DesktopLayerSurface_wl_surface_listener;
extern const struct zwlr_layer_surface_v1_listener DesktopLayerSurface_zwlr_layer_surface_listener;
