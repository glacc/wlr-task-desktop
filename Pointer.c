#include "Pointer.h"

#include <stdio.h>

void Pointer_GetWlObj(struct Pointer *pointer, struct wl_seat *seat)
{
    if (!pointer)
        return;

    pointer->wl_pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(pointer->wl_pointer, &Pointer_wl_listener, pointer);
}

void Pointer_DestroyWlObj(struct Pointer *pointer)
{
    if (!pointer)
        return;

    if (!pointer->wl_pointer)
        return;

    wl_pointer_destroy(pointer->wl_pointer);
    pointer->wl_pointer = NULL;
}

static void Pointer_Enter(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *surface,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct Pointer *pointer_state = data;
    pointer_state->wl_surface = surface;
}

static void Pointer_Leave(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *surface
)
{
    struct Pointer *pointer_state = data;
    pointer_state->wl_surface = NULL;
}

static void Pointer_Motion(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct Pointer *pointer_state = data;
    pointer_state->x = surface_x;
    pointer_state->y = surface_y;
    
    printf("motion event to (%d, %d)\n", surface_x, surface_y);
}

static void Pointer_Button(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    uint32_t time,
    uint32_t button,
    uint32_t state
)
{
    struct Pointer *pointer_state = data;
    const char *button_state_str[2] =
    {
        "released",
        "pressed",
    };

    printf(
        "button %u %s at surface position (%d, %d)\n",
        button, button_state_str[state], pointer_state->x, pointer_state->y
    );
}

static void Pointer_Axis(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis,
    wl_fixed_t value
)
{

}

static void Pointer_Frame(
    void *data,
    struct wl_pointer *wl_pointer
)
{

}

static void Pointer_AxisSource(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis_source
)
{

}

static void Pointer_AxisStop(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis
)
{

}

static void Pointer_AxisDiscrete(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    int32_t discrete
)
{

}

static void Pointer_AxisValue120(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    int32_t value120
)
{

}

static void Pointer_AxisRelativeDirection(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    uint32_t direction
)
{

}

static void Pointer_Warp(
    void *data,
    struct wl_pointer *wl_pointer,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{

}

const struct wl_pointer_listener Pointer_wl_listener =
{
    .enter = Pointer_Enter,
    .leave = Pointer_Leave,
    .motion = Pointer_Motion,
    .button = Pointer_Button,
    .axis = Pointer_Axis,
    .frame = Pointer_Frame,
    .axis_source = Pointer_AxisSource,
    .axis_stop = Pointer_AxisStop,
    .axis_discrete = Pointer_AxisDiscrete,
    .axis_value120 = Pointer_AxisValue120,
    .axis_relative_direction = Pointer_AxisRelativeDirection,
    .warp = Pointer_Warp,
};
