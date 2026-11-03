#pragma once

#include <wayland-client.h>

struct Pointer
{
    struct wl_pointer *wl_pointer;

    struct wl_surface *wl_surface;
    
    wl_fixed_t x;
    wl_fixed_t y;
};

extern void Pointer_GetWlObj(struct Pointer *pointer, struct wl_seat *seat);
extern void Pointer_DestroyWlObj(struct Pointer *pointer);

extern const struct wl_pointer_listener Pointer_wl_listener;
