#pragma once

#include <stdbool.h>

#include <wayland-client.h>

#include "HashSet.h"

struct Output
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
};

extern const struct wl_output_listener Output_wl_listener;

extern size_t Output_Hash(void *ptr);
extern bool Output_IsEqual(void *l, void *r);

extern struct Output *Output_New(void);
extern void Output_Destroy(struct Output *output);
