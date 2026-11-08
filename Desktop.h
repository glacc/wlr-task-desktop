#pragma once

#include <stdbool.h>

#include <wayland-client.h>

#include "HashSet.h"
#include "DesktopLayerSurface.h"

#include "Vector.h"

struct Desktop
{
    // output
    struct wl_output *wl_output;

    uint32_t wl_name;

    char *name;
    char *description;

    int32_t x, y;
    int32_t phy_w, phy_h;

    int32_t w, h;

    int32_t scale;

    // surface
    struct DesktopLayerSurface desktop_layer_surface;
    struct Vector damage_bounds; // typeof(SWRenderBoundI)
    bool damaged;

    // toplevel handle
    struct HashSet toplevel_handles;

    // selected handle and state
    struct ToplevelHandle *selected_handle;
    int icon_drag_start_x;
    int icon_drag_start_y;

    bool debug_key_pressed;
};

extern const struct wl_output_listener Desktop_output_wl_listener;

extern size_t Desktop_Hash(void *ptr);
extern bool Desktop_IsEqual(void *l, void *r);

extern struct Desktop *Desktop_Create(void);
extern void Desktop_Destroy(struct Desktop *desktop);

extern void Desktop_Update(struct Desktop *desktop);

extern bool Desktop_AddDamage(struct Desktop *desktop, int32_t x, int32_t y, int32_t w, int32_t h);
extern void Desktop_CheckRedrawAndTryCommit(struct Desktop *desktop);
