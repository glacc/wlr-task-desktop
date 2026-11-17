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

#include <xkbcommon/xkbcommon.h>

#include <wayland-client.h>

#include "double_buffer_surface.h"

#include "hashset.h"
#include "vector.h"

struct keyboard_key_state
{
    xkb_keysym_t keysym;

    bool is_down;

    bool pressed;
    uint32_t press_time;

    bool released;
    uint32_t release_time;

    bool repeated;
    uint32_t repeat_time;
};

struct keyboard_state
{
    struct seat *seat;

    bool keyboard_focused;
    uint32_t keyboard_enter_serial;
    uint32_t keyboard_leave_serial;

    struct hashset key_states;      // struct keyboard_key_state *
    struct vector keys_pressed;     // struct keyboard_key_state *
    struct vector keys_released;    // struct keyboard_key_state *
    struct vector keys_repeated;    // struct keyboard_key_state *
};

struct keyboard
{
    struct wl_keyboard *wl_keyboard;

    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;
    bool has_xkb_keymap;

    struct seat *seat;

    struct double_buffer_surface *surface_curr;

    struct hashset surface_visited;

    bool available;
    bool removed;
};

size_t keyboard_state_hash_function(void *ptr);
bool keyboard_state_is_equal(void *l, void *r);

#pragma region keyboard_state

extern void keyboard_state_init(struct keyboard_state *keyboard_state, struct seat *seat);
extern void keyboard_state_cleanup(struct keyboard_state *keyboard_state);

extern struct keyboard_state *keyboard_state_create(struct seat *seat);
extern void keyboard_state_destroy(struct keyboard_state *keyboard_state);

extern struct keyboard_key_state *keyboard_state_get_key_state(struct keyboard_state *keyboard_state, xkb_keysym_t keysym);

#pragma endregion

extern void keyboard_init(struct keyboard *keyboard, struct seat *seat);
extern void keyboard_cleanup(struct keyboard *keyboard);

extern void keyboard_assign_wl_keyboard(struct keyboard *keyboard, struct wl_keyboard *wl_keyboard);
extern void keyboard_remove_wl_keyboard(struct keyboard *keyboard);

extern void keyboard_manage_post_update(struct keyboard *keyboard);
