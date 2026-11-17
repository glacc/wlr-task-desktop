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

#include "buffer.h"
#include "pointer.h"

#include "SWRender/SWRender.h"

#include "vector.h"
#include "hashset.h"

struct double_buffer_surface
{
    struct wl_surface *wl_surface;

    struct buffer buffer[2];
    struct vector damage_rects[2];  // struct buffer_rect *
    unsigned int commit_count;

    int32_t w, h;

    SWRenderBuffer swrender_buffer;

    struct hashset keyboard_states; // struct keyboard_state *
    struct hashset pointer_states;  // struct pointer_state *

    bool valid;
};

extern void double_buffer_surface_init(struct double_buffer_surface *surface);
extern void double_buffer_surface_cleanup(struct double_buffer_surface *surface);

extern void double_buffer_surface_set_size(struct double_buffer_surface *surface, int32_t w, int32_t h);

extern void double_buffer_surface_damage(struct double_buffer_surface *surface, const struct buffer_rect *rect);
extern bool double_buffer_surface_is_any_buffer_available(struct double_buffer_surface *surface);
extern bool double_buffer_surface_try_commit(struct double_buffer_surface *surface);

#pragma region keyboard_state

extern struct keyboard_state *double_buffer_surface_get_keyboard_state(struct double_buffer_surface *surface, struct seat *seat);
extern struct keyboard_state *double_buffer_surface_create_or_get_keyboard_state(struct double_buffer_surface *surface, struct seat *seat);
extern void double_buffer_surface_remove_keyboard_state_by_seat(struct double_buffer_surface *surface, struct seat *seat);

#pragma endregion

#pragma region pointer_state

extern struct pointer_state *double_buffer_surface_get_pointer_state(struct double_buffer_surface *surface, struct seat *seat);
extern struct pointer_state *double_buffer_surface_create_or_get_pointer_state(struct double_buffer_surface *surface, struct seat *seat);
extern void double_buffer_surface_remove_pointer_state_by_seat(struct double_buffer_surface *surface, struct seat *seat);

#pragma endregion

extern void double_buffer_manage_post_update(struct double_buffer_surface *surface);
