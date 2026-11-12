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

#pragma once

#include <wayland-client.h>
#include "cursor-shape-v1-client-protocol.h"

#include "HashSet.h"

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

struct PointerBtnState
{
    uint32_t button;
    
    // state change
    bool is_down;

    bool pressed;
    wl_fixed_t pressed_x;
    wl_fixed_t pressed_y;
    int pressed_x_int;
    int pressed_y_int;
    uint32_t pressed_time;

    bool release;
    wl_fixed_t release_x;
    wl_fixed_t release_y;
    int release_x_int;
    int release_y_int;
    uint32_t release_time;

    // double click
    bool double_click;
    wl_fixed_t double_click_x;
    wl_fixed_t double_click_y;
    int double_click_x_int;
    int double_click_y_int;
    uint32_t double_click_time;

    // drag
    bool dragging;

    bool drag_start;
    wl_fixed_t drag_start_x;
    wl_fixed_t drag_start_y;
    int drag_start_x_int;
    int drag_start_y_int;
    uint32_t drag_start_time;

    bool drag_stop;
    uint32_t drag_stop_time;
};

struct PointerState
{
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

    bool pointer_inside;

    struct HashSet button_states;   // typeof(struct PointerBtnState *)
};

struct Pointer
{
    struct wl_pointer *wl_pointer;
    struct wp_cursor_shape_device_v1 *wp_cursor_shape_device;

    struct DoubleBufferedSurface *surface;

    struct HashSet surfaces_to_manage;  // typeof(struct DoubleBufferedSurface *)
};

extern struct PointerBtnState *PointerBtnState_Create(uint32_t button);
extern void PointerBtnState_Destroy(struct PointerBtnState* btn_state);

extern size_t PointerBtnState_Hash(void *btn_state);
extern bool PointerBtnState_IsEqual(void *l, void *r);

extern bool PointerState_Init(struct PointerState *pointer_state);
extern void PointerState_Cleanup(struct PointerState *pointer_state);

extern void PointerState_PostDispatch(struct PointerState *pointer_state);

extern bool Pointer_Init(struct Pointer *pointer);
extern void Pointer_Cleanup(struct Pointer *pointer);

extern void Pointer_GetWlObj(struct Pointer *pointer, struct wl_seat *seat);
extern void Pointer_DestroyWlObj(struct Pointer *pointer);

extern struct PointerBtnState *Pointer_GetBtnState(struct PointerState *pointer_state, uint32_t button);

extern const struct wl_pointer_listener Pointer_listener;
