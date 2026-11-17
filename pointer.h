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

#pragma once

#include <wayland-client.h>
#include "cursor-shape-v1-client-protocol.h"

#include "hashset.h"

// double click distance threshold
#define POINTER_DOUBLE_CLICK_DIST_THRESHOLD_1X 5
#define POINTER_DOUBLE_CLICK_DIST_THRESHOLD_SQ_WL_FIXED \
    ((POINTER_DOUBLE_CLICK_DIST_THRESHOLD_1X * POINTER_DOUBLE_CLICK_DIST_THRESHOLD_1X) << 8)

// drag start distance threshold
#define POINTER_DRAG_START_THRESHOLD_1X 5
#define POINTER_DRAG_START_THRESHOLD_SQ_WL_FIXED \
    ((POINTER_DRAG_START_THRESHOLD_1X * POINTER_DRAG_START_THRESHOLD_1X) << 8)

// TODO: make this configurable?
#define POINTER_DOUBLE_CLICK_TIME_MS 400

struct pointer_button_state
{
    uint32_t button;
    
    bool is_down;

    bool pressed;
    wl_fixed_t press_x;
    wl_fixed_t press_y;
    int press_x_int;
    int press_y_int;
    uint32_t press_time;

    bool released;
    wl_fixed_t release_x;
    wl_fixed_t release_y;
    int release_x_int;
    int release_y_int;
    uint32_t release_time;

    // double click
    bool double_clicked;
    wl_fixed_t double_click_x;
    wl_fixed_t double_click_y;
    int double_click_x_int;
    int double_click_y_int;
    uint32_t double_click_time;

    // drag
    bool is_dragging;

    bool drag_started;
    wl_fixed_t drag_start_x;
    wl_fixed_t drag_start_y;
    int drag_start_x_int;
    int drag_start_y_int;
    uint32_t drag_start_time;

    bool drag_stopped;
    wl_fixed_t drag_stop_x;
    wl_fixed_t drag_stop_y;
    int drag_stop_x_int;
    int drag_stop_y_int;
    uint32_t drag_stop_time;
};

struct pointer_state
{
    struct seat *seat;

    bool pointer_inside;
    uint32_t pointer_enter_serial;
    uint32_t pointer_leave_serial;

    // surface position
    wl_fixed_t x;
    wl_fixed_t y;
    int x_int;
    int y_int;
    uint32_t motion_time;

    wl_fixed_t x_old;
    wl_fixed_t y_old;
    int x_old_int;
    int y_old_int;
    uint32_t motion_old_time;

    struct hashset button_states;       // struct pointer_button_state *
};

struct pointer
{
    struct wl_pointer *wl_pointer;
    struct wp_cursor_shape_device_v1 *wp_cursor_shape_device;

    struct seat *seat;

    struct double_buffer_surface *surface_curr;

    struct hashset surface_visited;

    bool available;
    bool removed;
};

size_t pointer_state_hash_function(void *ptr);
bool pointer_state_is_equal(void *l, void *r);

#pragma region pointer_state

extern void pointer_state_init(struct pointer_state *pointer_state, struct seat *seat);
extern void pointer_state_cleanup(struct pointer_state *pointer_state);

extern struct pointer_state *pointer_state_create(struct seat *seat);
extern void pointer_state_destroy(struct pointer_state *pointer_state);

extern struct pointer_button_state *pointer_state_get_button_state(struct pointer_state *pointer_state, uint32_t button);
extern void pointer_state_set_cursor(struct pointer_state *pointer_state, enum wp_cursor_shape_device_v1_shape shape);

#pragma endregion

extern void pointer_init(struct pointer *pointer, struct seat *seat);
extern void pointer_cleanup(struct pointer *pointer);

extern void pointer_assign_wl_pointer(struct pointer *pointer, struct wl_pointer *wl_pointer);
extern void pointer_remove_wl_pointer(struct pointer *pointer);

extern void pointer_manage_post_update(struct pointer *pointer);
