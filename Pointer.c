/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#include "Pointer.h"

#include <stdio.h>
#include <malloc.h>

#include "DoubleBufferedSurface.h"

#include "Utility.h"

#include "Globals.h"

int Pointer_debug_verbosity = 1;

static void Pointer_SetBtnStateInternal(struct PointerState *pointer_state, uint32_t button, bool down, uint32_t time)
{
    struct PointerBtnState *btn_state = NULL;

    if (!(btn_state = Pointer_GetBtnState(pointer_state, button)))
        return;

    btn_state->pressed = down;
    btn_state->release = !down;
    btn_state->is_down = down;

    if (down)
    {
        // double click check
        uint32_t time_between_last_click = time - btn_state->pressed_time;

        if (time_between_last_click < POINTER_DOUBLE_CLICK_TIME_MS)
        {
            wl_fixed_t dist_sq_between_presses = Utility_DistSqBetweenVecWlFixed(
                btn_state->pressed_x,
                btn_state->pressed_y,
                pointer_state->x,
                pointer_state->y
            );

            // not double click if distance too far between two presses
            if (dist_sq_between_presses < POINTER_DOUBLE_CLICK_DIST_THRESHOLD_SQ_WL_FIXED)
            {
                btn_state->double_click = true;
            
                btn_state->double_click_x = pointer_state->x;
                btn_state->double_click_x = pointer_state->y;
                btn_state->double_click_x_int = pointer_state->x_int;
                btn_state->double_click_x_int = pointer_state->y_int;
                btn_state->double_click_time = time;
            }
        }

        // set press state
        btn_state->pressed_x = pointer_state->x;
        btn_state->pressed_y = pointer_state->y;
        btn_state->pressed_x_int = pointer_state->x_int;
        btn_state->pressed_y_int = pointer_state->y_int;
        btn_state->pressed_time = time;
    }
    else
    {
        btn_state->release_x = pointer_state->x;
        btn_state->release_y = pointer_state->y;
        btn_state->release_x_int = pointer_state->x_int;
        btn_state->release_y_int = pointer_state->y_int;
        btn_state->release_time = time;

        btn_state->dragging = false;
        
        if (btn_state->dragging)
        {
            btn_state->drag_stop = true;
            btn_state->drag_stop_time = time;
        }

    }
}

#pragma region WaylandCallbacks

static void Pointer_Enter(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *wl_surface,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct Pointer *pointer = data;
    struct DoubleBufferedSurface *surface = wl_surface_get_user_data(wl_surface);

    pointer->surface = surface;

    if (!surface)
        return;
    
    HashSet_Add(&pointer->surfaces_to_manage, surface, NULL);
    surface->pointer = pointer;
    surface->pointer_enter_serial = serial;
    surface->pointer_state.pointer_inside = true;

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void Pointer_Leave(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *wl_surface
)
{
    struct Pointer *pointer = data;
    
    if (!wl_surface)
        return;

    struct DoubleBufferedSurface *surface = wl_surface_get_user_data(wl_surface);

    if (pointer->surface == surface)
        pointer->surface = NULL;

    if (surface)
        surface->pointer_state.pointer_inside = false;

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void Pointer_Motion(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct Pointer *pointer = data;
    
    struct DoubleBufferedSurface *surface = pointer->surface;

    if (!surface)
        return;

    int surface_x_int = Utility_WlFixedToInt(surface_x);
    int surface_y_int = Utility_WlFixedToInt(surface_y);

    struct PointerState *pointer_state = &surface->pointer_state;

    pointer_state->motion_time = time;
    pointer_state->x = surface_x;
    pointer_state->y = surface_y;
    pointer_state->x_int = surface_x_int;
    pointer_state->y_int = surface_y_int;

    struct HashSet_Iter iter = { };
    if (HashSet_IterInit(&iter, &pointer_state->button_states))
    {
        struct PointerBtnState *btn_state = NULL;

        while ((btn_state = HashSet_IterNext(&iter)))
        {
            if (!btn_state->is_down)
                continue;

            if (btn_state->dragging)
                continue;

            wl_fixed_t dist_sq_from_press = Utility_DistSqBetweenVecWlFixed(
                surface_x,
                surface_y,
                btn_state->pressed_x,
                btn_state->pressed_y
            );

            if (dist_sq_from_press > POINTER_DRAG_START_THRESHOLD_SQ_WL_FIXED)
            {
                btn_state->drag_start = true;
                btn_state->dragging = true;

                btn_state->drag_start_x = surface_x;
                btn_state->drag_start_y = surface_y;
                btn_state->drag_start_x_int = surface_x_int;
                btn_state->drag_start_y_int = surface_y_int;

                btn_state->drag_start_time = time;
            }
        }
    }

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
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
    struct Pointer *pointer = data;
    
    struct DoubleBufferedSurface *surface = pointer->surface;

    if (!surface)
        return;

    struct PointerState *pointer_state = &surface->pointer_state;

    Pointer_SetBtnStateInternal(pointer_state, button, (state == WL_POINTER_BUTTON_STATE_PRESSED), time);

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

#pragma region WaylandPointerAxisCallbacks

static void Pointer_Axis(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis,
    wl_fixed_t value
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

#pragma endregion

static void Pointer_Warp(
    void *data,
    struct wl_pointer *wl_pointer,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    // current left empty

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void Pointer_Frame(
    void *data,
    struct wl_pointer *wl_pointer
)
{
    // pointer event frame ends
}

const struct wl_pointer_listener Pointer_listener =
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

#pragma endregion

#pragma region PointerBtnState

struct PointerBtnState *PointerBtnState_Create(uint32_t button)
{
    struct PointerBtnState *btn_state = calloc(1, sizeof(*btn_state));
    btn_state->button = button;
    return btn_state;
}

void PointerBtnState_Destroy(struct PointerBtnState* btn_state)
{
    free(btn_state);
}

#pragma region PointerBtnStateHashSetOperators

size_t PointerBtnState_Hash(void *btn_state)
{
    return ((struct PointerBtnState *)btn_state)->button;
}

bool PointerBtnState_IsEqual(void *l, void *r)
{
    struct PointerBtnState *btn_state_l = (struct PointerBtnState *)l;
    struct PointerBtnState *btn_state_r = (struct PointerBtnState *)r;
    return (btn_state_l->button == btn_state_r->button);
}

#pragma endregion

#pragma endregion

#pragma region PointerState

void PointerState_Init(struct PointerState *pointer_state)
{
    pointer_state->x_old = pointer_state->x = 0;
    pointer_state->y_old = pointer_state->y = 0;
    pointer_state->pointer_inside = false;

    HashSet_Init(&pointer_state->button_states);
    pointer_state->button_states.Hash = PointerBtnState_Hash;
    pointer_state->button_states.IsEqual = PointerBtnState_IsEqual;
}

void PointerState_Cleanup(struct PointerState *pointer_state)
{
    struct HashSet_Iter iter = { };
    if (HashSet_IterInit(&iter, &pointer_state->button_states))
    {
        struct PointerBtnState *btn_state = NULL;

        while ((btn_state = HashSet_IterDel(&iter)))
            PointerBtnState_Destroy(btn_state);
    }
    HashSet_Cleanup(&pointer_state->button_states);
}

void PointerState_PostDispatch(struct PointerState *pointer_state)
{
    pointer_state->x_old = pointer_state->x;
    pointer_state->y_old = pointer_state->y;
    pointer_state->x_old_int = pointer_state->x_int;
    pointer_state->y_old_int = pointer_state->y_int;
    pointer_state->motion_old_time = pointer_state->motion_time;

    struct HashSet_Iter iter = { };
    if (HashSet_IterInit(&iter, &pointer_state->button_states))
    {
        struct PointerBtnState *btn_state = NULL;

        while ((btn_state = HashSet_IterNext(&iter)))
        {
            btn_state->pressed = false;
            btn_state->release = false;
            btn_state->double_click = false;

            btn_state->drag_start = false;
            btn_state->drag_stop = false;
        }
    }
}

#pragma endregion

void Pointer_Init(struct Pointer *pointer)
{
    HashSet_Init(&pointer->surfaces_to_manage);
}

void Pointer_Cleanup(struct Pointer *pointer)
{
    // unref
    // can i avoid this kind of bidirection ref?
    struct HashSet_Iter iter = { };
    struct DoubleBufferedSurface *surface = NULL;

    HashSet_IterInit(&iter, &pointer->surfaces_to_manage);
    while ((surface = HashSet_IterDel(&iter)))
    {
        if (surface->pointer == pointer)
            surface->pointer = NULL;
    }
    HashSet_Cleanup(&pointer->surfaces_to_manage);

    Pointer_DestroyWlObj(pointer);
}

void Pointer_SetWlObj(struct Pointer *pointer, struct wl_seat *seat)
{
    pointer->wl_pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(pointer->wl_pointer, &Pointer_listener, pointer);

    if (!wp_cursor_shape_manager)
        return;

    pointer->wp_cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(wp_cursor_shape_manager, pointer->wl_pointer);
}

void Pointer_DestroyWlObj(struct Pointer *pointer)
{
    wp_cursor_shape_device_v1_destroy(pointer->wp_cursor_shape_device);

    if (!pointer->wl_pointer)
        return;

    wl_pointer_destroy(pointer->wl_pointer);
    pointer->wl_pointer = NULL;
}

struct PointerBtnState *Pointer_GetBtnState(struct PointerState *pointer_state, uint32_t button)
{
    struct PointerBtnState btn_state_to_find = { .button = button };
    struct PointerBtnState *btn_state = NULL;

    if ((btn_state = HashSet_Find(&pointer_state->button_states, &btn_state_to_find)))
        return btn_state;

    btn_state = PointerBtnState_Create(button);
    HashSet_Add(&pointer_state->button_states, btn_state, NULL);

    return btn_state;
}
