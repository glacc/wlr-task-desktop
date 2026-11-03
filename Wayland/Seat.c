#include "Seat.h"

#include <stdio.h>

#include "Wayland.h"

static void Seat_Capabilities(
    void *data,
    struct wl_seat *wl_seat,
    uint32_t capabilities
)
{
    struct Wayland *wayland = data;

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
        Keyboard_GetWlObj(&wayland->keyboard, wl_seat);
    else
        Keyboard_DestroyWlObj(&wayland->keyboard);
    
    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        Pointer_GetWlObj(&wayland->pointer, wl_seat);
    else
        Pointer_DestroyWlObj(&wayland->pointer);
}

static void Seat_Name(
    void *data,
    struct wl_seat *wl_seat,
    const char *name
)
{
    printf("seat name: %s\n", name);
}

const struct wl_seat_listener Seat_wl_listener =
{
    .capabilities = Seat_Capabilities,
    .name = Seat_Name,
};
