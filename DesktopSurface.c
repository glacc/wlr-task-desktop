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

#include "DesktopSurface.h"

#include <stdio.h>
#include <memory.h>

#include "Globals.h"

int DesktopLayerSurface_debug_verbosity = 1;

static void DesktopSurface_Surface_Enter(
    void *data,
    struct wl_surface *wl_surface,
    struct wl_output *output
)
{
    // currently left empty

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void DesktopSurface_Surface_Leave(
    void *data,
    struct wl_surface *wl_surface,
    struct wl_output *output
)
{
    // currently left empty

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void DesktopSurface_Surface_PreferredBufferScale
(
    void *data,
    struct wl_surface *wl_surface,
    int32_t factor
)
{
    // currently left empty

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void DesktopSurface_Surface_PreferredBufferTransform
(
    void *data,
    struct wl_surface *wl_surface,
    uint32_t transform
)
{
    // currently left empty

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void DesktopSurface_Configure(
    void *data,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
    uint32_t serial,
    uint32_t width,
    uint32_t height
)
{
    struct DesktopSurface *desktop_surface = data;

    // update size
    zwlr_layer_surface_v1_set_size(zwlr_layer_surface_v1, width, height);

    // ack
    zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);

    // update surface size and viewport dst
    DoubleBufferedSurface_SetSize(&desktop_surface->surface, width, height);
    wp_viewport_set_destination(desktop_surface->viewport, desktop_surface->surface.w, desktop_surface->surface.h);

    // ----------------------------- debug output -----------------------------
    if (DesktopLayerSurface_debug_verbosity < 1)
        return;

    printf(
        "layer surface configure:\n"
        "\tserial: %u\n"
        "\t%ux%u\n",
        serial, width, height
    );
}

static void DesktopLayerSurface_Closed(
    void *data,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1
)
{
    struct DesktopSurface *layer_surface = data;

    layer_surface->closed = true;

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

const struct wl_surface_listener DesktopSurface_surface_listener =
{
    .enter                      = DesktopSurface_Surface_Enter,
    .leave                      = DesktopSurface_Surface_Leave,
    .preferred_buffer_scale     = DesktopSurface_Surface_PreferredBufferScale,
    .preferred_buffer_transform = DesktopSurface_Surface_PreferredBufferTransform,
};

const struct zwlr_layer_surface_v1_listener DesktopSurface_layer_surface_listener =
{
    .configure = DesktopSurface_Configure,
    .closed    = DesktopLayerSurface_Closed,
};

void DesktopSurface_Init(struct DesktopSurface *desktop_surface, struct wl_output *wl_output)
{
    // create double buffered surface
    DoubleBufferedSurface_Init(&desktop_surface->surface);

    // viewport
    desktop_surface->viewport = wp_viewporter_get_viewport(wp_viewporter, desktop_surface->surface.wl_surface);

    desktop_surface->zwlr_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        zwlr_layer_shell,
        desktop_surface->surface.wl_surface,
        wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        "desktop"
    );

    // configuration
    zwlr_layer_surface_v1_set_margin(
        desktop_surface->zwlr_layer_surface,
        0, 0, 0, 0
    );
    zwlr_layer_surface_v1_set_anchor(
        desktop_surface->zwlr_layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
    );
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        desktop_surface->zwlr_layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND
    );
    zwlr_layer_surface_v1_add_listener(
        desktop_surface->zwlr_layer_surface, &DesktopSurface_layer_surface_listener, desktop_surface);

    // first commit
    wl_surface_commit(desktop_surface->surface.wl_surface);
    wl_display_roundtrip(wl_display);
}

void DesktopSurface_Cleanup(struct DesktopSurface *desktop_surface)
{
    zwlr_layer_surface_v1_destroy(desktop_surface->zwlr_layer_surface);

    wp_viewport_destroy(desktop_surface->viewport);

    DoubleBufferedSurface_Cleanup(&desktop_surface->surface);
}
