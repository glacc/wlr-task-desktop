#pragma once

#include <stdbool.h>

#include <wayland-client.h>

#include "Vector.h"

struct Wayland_Output
{
    struct wl_output *wl_output;

    uint32_t wl_name;

    int32_t x, y;
    int32_t phy_w, phy_h;

    int32_t w, h;

    int32_t scale;
};

extern const struct wl_output_listener Output_wl_listener;

extern bool Outputs_VectorInit(struct Vector *outputs);
extern void Outputs_VectorCleanup(struct Vector *outputs);
extern void Outputs_RemoveOutputByName(struct Vector *outputs, uint32_t name);
