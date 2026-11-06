#pragma once

#include <stdbool.h>

#include <wayland-client.h>

#include "HashSet.h"
#include "DesktopLayerSurface.h"

#include "Vector.h"

// TODO: make icon size configurable?
#define DESKTOP_DEFAULT_ICON_SIZE_X 80
#define DESKTOP_DEFAULT_ICON_SIZE_Y 96

struct Desktop
{
    struct wl_output *wl_output;

    uint32_t wl_name;

    char *name;
    char *description;

    int32_t x, y;
    int32_t phy_w, phy_h;

    int32_t w, h;

    int32_t scale;

    struct HashSet toplevel_handles;

    struct DesktopLayerSurface surface;

    struct Vector damage_bounds; // typeof(SWRenderBoundI)
    bool damaged;
};

extern const struct wl_output_listener Desktop_output_wl_listener;

extern size_t Desktop_Hash(void *ptr);
extern bool Desktop_IsEqual(void *l, void *r);

extern struct Desktop *Desktop_Create(void);
extern void Desktop_Destroy(struct Desktop *desktop);

extern void Desktop_Update(struct Desktop *desktop);

extern bool Desktop_AddDamage(struct Desktop *desktop, int32_t x, int32_t y, int32_t w, int32_t h);
extern void Desktop_CheckRedrawAndTryCommit(struct Desktop *desktop);
