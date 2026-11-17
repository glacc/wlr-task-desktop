/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#include "pointer.h"

#include <malloc.h>
#include <assert.h>

#include "main.h"
#include "seat.h"
#include "double_buffer_surface.h"

#include "debug.h"

#ifndef _POINTER_DEBUG_PRINT_MOTION_
#define _POINTER_DEBUG_PRINT_MOTION_ 0
#endif

#ifndef _POINTER_DEBUG_
#define _POINTER_DEBUG_ 0
#endif

#define POINTER_DEBUG_MOTION_PRINT_INTERVAL_MS 300

static uint32_t last_pointer_motion_print_time = 0;

static wl_fixed_t wl_fixed_distance_square(wl_fixed_t x1, wl_fixed_t y1, wl_fixed_t x2, wl_fixed_t y2)
{
    int64_t diff_x = x2 - x1;
    int64_t diff_y = y2 - y1;
    return (wl_fixed_t)((((diff_x * diff_x) + (diff_y * diff_y)) + 128) >> 8);
}

static int wl_fixed_to_int_rounded(wl_fixed_t wl_fixed)
{
    return (wl_fixed + 128) >> 8;
}

#pragma region pointer_button_state

#pragma region pointer_button_state_hashset_functions

static size_t pointer_button_state_hash_function(void *ptr)
{
    struct pointer_button_state *pointer_button_state = ptr;
    return pointer_button_state->button;
}

static bool pointer_button_state_is_equal(void *l, void *r)
{
    struct pointer_button_state *pointer_button_state_l = l;
    struct pointer_button_state *pointer_button_state_r = r;
    return (pointer_button_state_l->button == pointer_button_state_r->button);
}

#pragma endregion

static struct pointer_button_state *pointer_button_state_create(uint32_t button)
{
    struct pointer_button_state *pointer_button_state = calloc(1, sizeof(*pointer_button_state));
    pointer_button_state->button = button;
    return pointer_button_state;
}

static void pointer_button_state_destroy(struct pointer_button_state *pointer_button_state)
{
    free(pointer_button_state);
}

#pragma region pointer_button_state_actions

static void pointer_button_state_motion(struct pointer_button_state *pointer_button_state, struct pointer_state *pointer_state)
{
    if (!pointer_button_state->is_down || pointer_button_state->is_dragging)
        return;

    wl_fixed_t distance_square_to_press = wl_fixed_distance_square(
        pointer_state->x, pointer_state->y,
        pointer_button_state->press_x, pointer_button_state->press_y
    );
    
    if (distance_square_to_press >= POINTER_DRAG_START_THRESHOLD_SQ_WL_FIXED)
    {
        pointer_button_state->drag_started = true;
        pointer_button_state->drag_start_x = pointer_state->x;
        pointer_button_state->drag_start_y = pointer_state->y;
        pointer_button_state->drag_start_x_int = pointer_state->x_int;
        pointer_button_state->drag_start_y_int = pointer_state->y_int;
        pointer_button_state->drag_start_time = pointer_state->motion_time;

        pointer_button_state->is_dragging = true;

    #if _POINTER_DEBUG_ == 1
        debug_printf("seat '%s' pointer started dragging with button %u at x=%d, y=%d\n", debug_str_nullable(pointer_state->seat->identifier), pointer_button_state->button, pointer_button_state->drag_start_x_int, pointer_button_state->drag_start_y_int);
    #endif
    }
}

static void pointer_button_state_release(struct pointer_button_state *pointer_button_state, struct pointer_state *pointer_state, uint32_t time)
{
    if (!pointer_button_state->is_down)
        return;

    pointer_button_state->released = true;
    pointer_button_state->release_x = pointer_state->x;           
    pointer_button_state->release_y = pointer_state->y;        
    pointer_button_state->release_x_int = pointer_state->x_int;
    pointer_button_state->release_y_int = pointer_state->y_int;
    pointer_button_state->release_time = time;

    pointer_button_state->is_down = false;

#if _POINTER_DEBUG_ == 1
    debug_printf("seat '%s' pointer button %u released at x=%d, y=%d\n", debug_str_nullable(pointer_state->seat->identifier), pointer_button_state->button, pointer_button_state->release_x_int, pointer_button_state->release_y_int);
#endif
}

static void pointer_button_state_stop_drag(struct pointer_button_state *pointer_button_state, struct pointer_state *pointer_state, uint32_t time)
{
    if (!pointer_button_state->is_dragging)
        return;

    pointer_button_state->drag_stopped = true;
    pointer_button_state->drag_stop_x = pointer_state->x;
    pointer_button_state->drag_stop_y = pointer_state->y;
    pointer_button_state->drag_stop_x_int = pointer_state->x_int;
    pointer_button_state->drag_stop_y_int = pointer_state->y_int;
    pointer_button_state->drag_stop_time = time;

    pointer_button_state->is_dragging = false;

#if _POINTER_DEBUG_ == 1
    debug_printf("seat '%s' pointer stopped dragging with button %u at x=%d, y=%d\n", debug_str_nullable(pointer_state->seat->identifier), pointer_button_state->button, pointer_button_state->drag_stop_x_int, pointer_button_state->drag_stop_y_int);
#endif
}

#pragma endregion

static void pointer_button_state_manage_post_update(struct pointer_button_state *pointer_button_state)
{
    pointer_button_state->pressed  = false;
    pointer_button_state->released = false;
    
    pointer_button_state->double_clicked = false;

    pointer_button_state->drag_started = false;
    pointer_button_state->drag_stopped = false;
}

#pragma endregion

#pragma region pointer_state

#pragma region pointer_state_hashset_functions

size_t pointer_state_hash_function(void *ptr)
{
    struct pointer_state *pointer_state = ptr;
    return hashset_default_hash_function(pointer_state->seat);
}

bool pointer_state_is_equal(void *l, void *r)
{
    struct pointer_state *pointer_state_l = l;
    struct pointer_state *pointer_state_r = r;
    return (pointer_state_l->seat = pointer_state_r->seat);
}

#pragma endregion

void pointer_state_init(struct pointer_state *pointer_state, struct seat *seat)
{
    assert(pointer_state != NULL && seat != NULL);

    pointer_state->seat = seat;

    hashset_init(&pointer_state->button_states);
    pointer_state->button_states.hash_function = pointer_button_state_hash_function;
    pointer_state->button_states.is_equal = pointer_button_state_is_equal;
}

void pointer_state_cleanup(struct pointer_state *pointer_state)
{
    assert(pointer_state != NULL);

    struct hashset_iterator iterator_button_states = { };
    struct pointer_button_state *pointer_button_state = NULL;
    hashset_iterator_init(&iterator_button_states, &pointer_state->button_states);
    while ((pointer_button_state = hashset_iterator_next_deletion(&iterator_button_states)))
        pointer_button_state_destroy(pointer_button_state);

    hashset_cleanup(&pointer_state->button_states);
}

struct pointer_state *pointer_state_create(struct seat *seat)
{
    assert(seat != NULL);

    struct pointer_state *pointer_state = calloc(1, sizeof(*pointer_state));
    assert(pointer_state != NULL);

    pointer_state_init(pointer_state, seat);

    return pointer_state;
}

void pointer_state_destroy(struct pointer_state *pointer_state)
{
    assert(pointer_state != NULL);

    pointer_state_cleanup(pointer_state);

    free(pointer_state);
}

struct pointer_button_state *pointer_state_get_button_state(struct pointer_state *pointer_state, uint32_t button)
{
    assert(pointer_state != NULL);

    struct pointer_button_state pointer_button_state_to_find = { .button = button };
    struct pointer_button_state *pointer_button_state = hashset_find(&pointer_state->button_states, &pointer_button_state_to_find);
    if (pointer_button_state)
        return pointer_button_state;

    pointer_button_state = pointer_button_state_create(button);

    struct pointer_button_state *overwritten_pointer_button_state = NULL;
    hashset_insert(&pointer_state->button_states, pointer_button_state, (void **)&overwritten_pointer_button_state);
    assert(overwritten_pointer_button_state == NULL);

    return pointer_button_state;
}

#pragma region pointer_state_actions

static void pointer_state_press(struct pointer_state *pointer_state, uint32_t button, uint32_t time)
{
    struct pointer_button_state *pointer_button_state = pointer_state_get_button_state(pointer_state, button);

    if (pointer_button_state->is_down)
        return;

    bool is_in_double_click_interval = (time - pointer_button_state->press_time <= POINTER_DOUBLE_CLICK_TIME_MS);
    bool is_in_double_click_position_range = (wl_fixed_distance_square(pointer_button_state->press_x, pointer_button_state->press_y, pointer_state->x, pointer_state->y) <= POINTER_DOUBLE_CLICK_DIST_THRESHOLD_SQ_WL_FIXED);

    if (is_in_double_click_interval && is_in_double_click_position_range)
    {
        pointer_button_state->double_clicked = true;
        pointer_button_state->double_click_x = pointer_state->x;
        pointer_button_state->double_click_y = pointer_state->y;
        pointer_button_state->double_click_x_int = pointer_state->x_int;
        pointer_button_state->double_click_y_int = pointer_state->y_int;
        pointer_button_state->double_click_time = time;

    #if _POINTER_DEBUG_ == 1
        debug_printf("seat '%s' pointer button %u double clicked at x=%d, y=%d\n", debug_str_nullable(pointer_state->seat->identifier), pointer_button_state->button, pointer_button_state->double_click_x_int, pointer_button_state->double_click_y_int);
    #endif
    }

    pointer_button_state->pressed = true;
    pointer_button_state->press_x = pointer_state->x;
    pointer_button_state->press_y = pointer_state->y;
    pointer_button_state->press_x_int = pointer_state->x_int;
    pointer_button_state->press_y_int = pointer_state->y_int;
    pointer_button_state->press_time = time;

    pointer_button_state->is_down = true;

#if _POINTER_DEBUG_ == 1
    debug_printf("seat '%s' pointer button %u pressed at x=%d, y=%d\n", debug_str_nullable(pointer_state->seat->identifier), pointer_button_state->button, pointer_button_state->press_x_int, pointer_button_state->press_y_int);
#endif
}

static void pointer_state_motion(struct pointer_state *pointer_state, wl_fixed_t x, wl_fixed_t y, uint32_t time)
{
    // lazy pointer_inside update to prevent velocity calculation problems

    int x_int = wl_fixed_to_int_rounded(x);
    int y_int = wl_fixed_to_int_rounded(y);

    pointer_state->x = x;
    pointer_state->y = y;
    pointer_state->x_int = x_int;
    pointer_state->y_int = y_int;
    pointer_state->motion_time = time;

    if (!pointer_state->pointer_inside)
    {
        pointer_state->x_old = x;
        pointer_state->y_old = y;
        pointer_state->x_old_int = x_int;
        pointer_state->y_old_int = y_int;
        pointer_state->motion_time = time;
    }
    
    struct hashset_iterator iterator_button_states = { };
    struct pointer_button_state *pointer_button_state = NULL;
    hashset_iterator_init(&iterator_button_states, &pointer_state->button_states);
    while ((pointer_button_state = hashset_iterator_next(&iterator_button_states)))
        pointer_button_state_motion(pointer_button_state, pointer_state);

    pointer_state->pointer_inside = true;

#if _POINTER_DEBUG_ == 1 && _POINTER_DEBUG_PRINT_MOTION_ == 1
    if (time - last_pointer_motion_print_time >= POINTER_DEBUG_MOTION_PRINT_INTERVAL_MS)
    {
        debug_printf("seat '%s' pointer motion x=%d, y=%d\n", debug_str_nullable(pointer_state->seat->identifier), pointer_state->x_int, pointer_state->y_int);
        last_pointer_motion_print_time = time;
    }
#endif
}

static void pointer_state_release(struct pointer_state *pointer_state, uint32_t button, uint32_t time)
{
    struct pointer_button_state *pointer_button_state = pointer_state_get_button_state(pointer_state, button);

    pointer_button_state_release(pointer_button_state, pointer_state, time);
    pointer_button_state_stop_drag(pointer_button_state, pointer_state, time);
}

static void pointer_state_leave(struct pointer_state *pointer_state)
{
    struct hashset_iterator iterator_button_states = { };
    struct pointer_button_state *pointer_button_state = NULL;
    hashset_iterator_init(&iterator_button_states, &pointer_state->button_states);
    while ((pointer_button_state = hashset_iterator_next(&iterator_button_states)))
    {
        pointer_button_state_release(pointer_button_state, pointer_state, pointer_state->motion_time);
        pointer_button_state_stop_drag(pointer_button_state, pointer_state, pointer_state->motion_time);
    }

    pointer_state->pointer_inside = false;
}

#pragma endregion

void pointer_state_set_cursor(struct pointer_state *pointer_state, enum wp_cursor_shape_device_v1_shape shape)
{
    if (!pointer_state->pointer_inside || !pointer_state->seat->pointer.available || pointer_state->seat->removed)
        return;

    wp_cursor_shape_device_v1_set_shape(pointer_state->seat->pointer.wp_cursor_shape_device, pointer_state->pointer_enter_serial, shape);
}

static void pointer_state_manage_post_update(struct pointer_state *pointer_state)
{
    pointer_state->x_old = pointer_state->x;
    pointer_state->y_old = pointer_state->y;
    pointer_state->x_old_int = pointer_state->x_int;
    pointer_state->y_old_int = pointer_state->y_int;
    pointer_state->motion_old_time = pointer_state->motion_time;

    struct hashset_iterator iterator_button_states = { };
    struct pointer_button_state *button_state = NULL;
    hashset_iterator_init(&iterator_button_states, &pointer_state->button_states);
    while ((button_state = hashset_iterator_next(&iterator_button_states)))
        pointer_button_state_manage_post_update(button_state);
}

#pragma endregion

// TODO: when to implement axis functionality?

#pragma region pointer_listeners

static void pointer_enter(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *wl_surface,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct pointer *pointer = data;

    if (!wl_surface)
        return;

    pointer->surface_curr = wl_surface_get_user_data(wl_surface);

    struct pointer_state *pointer_state = double_buffer_surface_create_or_get_pointer_state(pointer->surface_curr, pointer->seat);
    pointer_state->pointer_enter_serial = serial;

    hashset_insert(&pointer->surface_visited, pointer->surface_curr, NULL);
}

static void pointer_leave(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *wl_surface
)
{
    struct pointer *pointer = data;
    
    if (!pointer->surface_curr)
        return;
    
    struct pointer_state *pointer_state = double_buffer_surface_create_or_get_pointer_state(pointer->surface_curr, pointer->seat);
    pointer_state->pointer_leave_serial = serial;
    pointer_state_leave(pointer_state);

    pointer->surface_curr = NULL;
}

static void pointer_motion(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct pointer *pointer = data;
    if (!pointer->surface_curr)
        return;

    struct pointer_state *pointer_state = double_buffer_surface_create_or_get_pointer_state(pointer->surface_curr, pointer->seat);
    pointer_state_motion(pointer_state, surface_x, surface_y, time);
}

static void pointer_warp(
    void *data,
    struct wl_pointer *wl_pointer,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    // not implemented yet
}

static void pointer_button(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    uint32_t time,
    uint32_t button,
    uint32_t state
)
{
    struct pointer *pointer = data;

    if (!pointer->surface_curr)
        return;

    struct pointer_state *pointer_state = double_buffer_surface_create_or_get_pointer_state(pointer->surface_curr, pointer->seat);

    if (state == WL_POINTER_BUTTON_STATE_PRESSED)
        pointer_state_press(pointer_state, button, time);
    else if (state == WL_POINTER_BUTTON_STATE_RELEASED)
        pointer_state_release(pointer_state, button, time);
}

#pragma region pointer_axis

static void pointer_axis(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis,
    wl_fixed_t value
)
{
    // not implemented yet
}

static void pointer_axis_source(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis_source
)
{
    // not implemented yet
}

static void pointer_axis_stop(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis
)
{
    // not implemented yet
}

static void pointer_axis_discrete(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    int32_t discrete
)
{
    // not implemented yet
}

static void pointer_axis_value120(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    int32_t value120
)
{
    // not implemented yet
}

static void pointer_axis_relative_direction(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    uint32_t direction
)
{
    // not implemented yet
}

#pragma endregion

static void pointer_frame(
    void *data,
    struct wl_pointer *wl_pointer
)
{
    // not implemented yet
}

static const struct wl_pointer_listener pointer_listener =
{
    .enter                   = pointer_enter,
    .leave                   = pointer_leave,
    .motion                  = pointer_motion,
    .button                  = pointer_button,
    .frame                   = pointer_frame,
    .axis                    = pointer_axis,                    // not implemented yet
    .axis_source             = pointer_axis_source,             // not implemented yet
    .axis_stop               = pointer_axis_stop,               // not implemented yet
    .axis_discrete           = pointer_axis_discrete,           // not implemented yet
    .axis_value120           = pointer_axis_value120,           // not implemented yet
    .axis_relative_direction = pointer_axis_relative_direction, // not implemented yet
    .warp                    = pointer_warp,                    // not implemented yet
};

#pragma endregion

void pointer_init(struct pointer *pointer, struct seat *seat)
{
    assert(pointer != NULL && seat != NULL);

    pointer->seat = seat;

    hashset_init(&pointer->surface_visited);
}

void pointer_cleanup(struct pointer *pointer)
{
    assert(pointer != NULL);

    struct hashset_iterator iterator_surfaces = { };
    struct double_buffer_surface *surface = NULL;
    hashset_iterator_init(&iterator_surfaces, &pointer->surface_visited);
    while ((surface = hashset_iterator_next_deletion(&iterator_surfaces)))
        double_buffer_surface_remove_pointer_state_by_seat(surface, pointer->seat);
    hashset_cleanup(&pointer->surface_visited);
}

void pointer_assign_wl_pointer(struct pointer *pointer, struct wl_pointer *wl_pointer)
{
    assert(pointer != NULL && wl_pointer != NULL);

    pointer_remove_wl_pointer(pointer);

    pointer->wl_pointer = wl_pointer;
    wl_pointer_add_listener(pointer->wl_pointer, &pointer_listener, pointer);

    if (wp_cursor_shape_manager)
        pointer->wp_cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(wp_cursor_shape_manager, pointer->wl_pointer);

    pointer->available = true;
}

void pointer_remove_wl_pointer(struct pointer *pointer)
{
    assert(pointer != NULL);

    if (!pointer->wl_pointer)
        return;

    if (pointer->wp_cursor_shape_device)
        wp_cursor_shape_device_v1_destroy(pointer->wp_cursor_shape_device);

    if (wl_pointer_get_version(pointer->wl_pointer) >= WL_POINTER_RELEASE_SINCE_VERSION)
        wl_pointer_release(pointer->wl_pointer);
    else
        wl_pointer_destroy(pointer->wl_pointer);

    pointer->wl_pointer = NULL;
    pointer->wp_cursor_shape_device = NULL;

    // act as leaving current interacting surface
#if 1
    if (pointer->surface_curr)
    {
        struct pointer_state *pointer_state = double_buffer_surface_get_pointer_state(pointer->surface_curr, pointer->seat);

        if (pointer_state)
            pointer_state_leave(pointer_state);
    }
#else
    struct hashset_iterator iterator_surface_visited = { };
    struct double_buffer_surface *surface = NULL;
    hashset_iterator_init(&iterator_surface_visited, &pointer->surface_visited);
    while ((surface = hashset_iterator_next_deletion(&iterator_surface_visited)))
    {
        struct pointer_state *pointer_state = double_buffer_surface_get_pointer_state(surface, pointer);

        if (!pointer_state)
            continue;

        pointer_state_leave(pointer_state);
    }
#endif

    pointer->removed = true;
}

void pointer_manage_post_update(struct pointer *pointer)
{
    assert(pointer != NULL);

    struct hashset_iterator iterator_surfaces = { };
    struct double_buffer_surface *surface = NULL;
    hashset_iterator_init(&iterator_surfaces, &pointer->surface_visited);
    while ((surface = hashset_iterator_next(&iterator_surfaces)))
    {
        struct pointer_state *pointer_state = double_buffer_surface_get_pointer_state(surface, pointer->seat);
        
        if (pointer_state)
            pointer_state_manage_post_update(pointer_state);
    }

    if (pointer->removed)
        pointer->available = false;
    
    pointer->removed = false;
}
