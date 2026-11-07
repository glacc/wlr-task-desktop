#include "Seat.h"

#include <stdio.h>

#include "Keyboard.h"
#include "Pointer.h"

#include "Globals.h"

static void Seat_Capabilities(
    void *data,
    struct wl_seat *wl_seat,
    uint32_t capabilities
)
{
    struct AppState *app_state = data;

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
        Keyboard_GetWlObj(&global_keyboard, wl_seat);
    else
        Keyboard_DestroyWlObj(&global_keyboard);
    
    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        Pointer_GetWlObj(&global_pointer, wl_seat);
    else
        Pointer_DestroyWlObj(&global_pointer);
}

static void Seat_Name(
    void *data,
    struct wl_seat *wl_seat,
    const char *name
)
{
    printf("seat name: %s\n", name);
}

const struct wl_seat_listener Seat_listener =
{
    .capabilities = Seat_Capabilities,
    .name = Seat_Name,
};
