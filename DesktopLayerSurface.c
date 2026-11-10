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

#include "DesktopLayerSurface.h"

#include <stdio.h>
#include <memory.h>

#include "Globals.h"

static void DesktopLayerSurface_Surface_Enter(
    void *data,
    struct wl_surface *wl_surface,
    struct wl_output *output
)
{
    puts("surface entered output");
}

static void DesktopLayerSurface_Surface_Leave(
    void *data,
    struct wl_surface *wl_surface,
    struct wl_output *output
)
{
    puts("surface leaved output");
}

static void DesktopLayerSurface_Surface_PreferredBufferScale
(
    void *data,
    struct wl_surface *wl_surface,
    int32_t factor
)
{
    printf("surface preferred buf scale: %d\n", factor);
}

static void DesktopLayerSurface_Surface_PreferredBufferTransform
(
    void *data,
    struct wl_surface *wl_surface,
    uint32_t transform
)
{
    printf("surface preferred buf transform: %u\n", transform);
}

static void DesktopLayerSurface_Configure(
    void *data,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
    uint32_t serial,
    uint32_t width,
    uint32_t height
)
{
    struct DesktopLayerSurface *layer_surface = data;

    // update size
    layer_surface->w = width;
    layer_surface->h = height;
    
    zwlr_layer_surface_v1_set_size(zwlr_layer_surface_v1, width, height);

    // ack
    zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);

    // update surface size and viewport dst
    DoubleBufferedSurface_SetSize(&layer_surface->double_buffered_surface, width, height);
    wp_viewport_set_destination(layer_surface->viewport, layer_surface->w, layer_surface->h);

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
    struct DesktopLayerSurface *layer_surface = data;

    layer_surface->closed = true;

    puts("layer surface closed");
}

const struct wl_surface_listener DesktopLayerSurface_wl_surface_listener =
{
    .enter = DesktopLayerSurface_Surface_Enter,
    .leave = DesktopLayerSurface_Surface_Leave,
    .preferred_buffer_scale = DesktopLayerSurface_Surface_PreferredBufferScale,
    .preferred_buffer_transform = DesktopLayerSurface_Surface_PreferredBufferTransform,
};

const struct zwlr_layer_surface_v1_listener DesktopLayerSurface_zwlr_layer_surface_listener =
{
    .configure = DesktopLayerSurface_Configure,
    .closed = DesktopLayerSurface_Closed,
};

bool DesktopLayerSurface_Init(struct DesktopLayerSurface *layer_surface, struct wl_output *output)
{
    if (!layer_surface)
        return false;

    // create double buffered surface
    if (!DoubleBufferedSurface_Init(&layer_surface->double_buffered_surface))
        return false;

    // viewport
    if (!(layer_surface->viewport = wp_viewporter_get_viewport(wp_viewporter, DesktopLayerSurface_GetWlSurface(layer_surface))))
    {
        DoubleBufferedSurface_Cleanup(&layer_surface->double_buffered_surface);
        return false;
    }

    // create layer surface
    layer_surface->zwlr_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        zwlr_layer_shell,
        DesktopLayerSurface_GetWlSurface(layer_surface),
        output,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        "desktop"
    );

    if (!layer_surface->zwlr_layer_surface)
    {
        DoubleBufferedSurface_Cleanup(&layer_surface->double_buffered_surface);
        return false;
    }

    // configuration
    zwlr_layer_surface_v1_set_margin(
        layer_surface->zwlr_layer_surface,
        0, 0, 0, 0
    );
    zwlr_layer_surface_v1_set_anchor(
        layer_surface->zwlr_layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
    );
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface->zwlr_layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND
    );
    zwlr_layer_surface_v1_add_listener(
        layer_surface->zwlr_layer_surface, &DesktopLayerSurface_zwlr_layer_surface_listener, layer_surface);

    wl_surface_commit(layer_surface->double_buffered_surface.surface);
    wl_display_roundtrip(wl_display);

    return true;
}

void DesktopLayerSurface_Cleanup(struct DesktopLayerSurface *layer_surface)
{
    if (!layer_surface)
        return;

    if (layer_surface->zwlr_layer_surface)
    {
        zwlr_layer_surface_v1_destroy(layer_surface->zwlr_layer_surface);
        layer_surface->zwlr_layer_surface = NULL;
    }

    if (layer_surface->viewport)
    {
        wp_viewport_destroy(layer_surface->viewport);
        layer_surface->viewport = NULL;
    }

    DoubleBufferedSurface_Cleanup(&layer_surface->double_buffered_surface);

    layer_surface->closed = true;
}

struct wl_surface *DesktopLayerSurface_GetWlSurface(struct DesktopLayerSurface *layer_surface)
{
    if (!layer_surface || !layer_surface->double_buffered_surface.surface)
        return NULL;

    return layer_surface->double_buffered_surface.surface;
}
