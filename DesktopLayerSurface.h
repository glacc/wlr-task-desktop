#pragma once

#include <wayland-client.h>

#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "AppState.h"
#include "DoubleBufferedSurface.h"

struct DesktopLayerSurface
{
    struct DoubleBufferedSurface surface;

    struct AppState *app_state;

    struct wp_viewport *viewport;
    struct zwlr_layer_surface_v1 *zwlr_layer_surface;

    uint32_t w;
    uint32_t h;

    bool closed;
};

extern bool DesktopLayerSurface_Init(struct DesktopLayerSurface *layer_surface, struct AppState *app_state, struct wl_output *output);
extern void DesktopLayerSurface_Cleanup(struct DesktopLayerSurface *layer_surface);

extern struct wl_surface *DesktopLayerSurface_GetWlSurface(struct DesktopLayerSurface *layer_surface);

extern const struct wl_surface_listener DesktopLayerSurface_wl_surface_listener;
extern const struct zwlr_layer_surface_v1_listener DesktopLayerSurface_zwlr_layer_surface_listener;
