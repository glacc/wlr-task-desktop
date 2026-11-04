#pragma once

#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct LayerSurface
{
    struct wl_surface *wl_surface;
    struct zwlr_layer_surface_v1 *zwlr_layer_surface;

    uint32_t w;
    uint32_t h;
};

extern const struct wl_surface_listener LayerSurface_wl_surface_listener;
extern const struct zwlr_layer_surface_v1_listener LayerSurface_zwlr_layer_surface_listener;
