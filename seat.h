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

#include <stdint.h>
#include <stddef.h>

#include <wayland-client.h>
#include "cursor-shape-v1-client-protocol.h"

#include "pointer.h"
#include "keyboard.h"

#include "hashset.h"

struct seat
{
    struct wl_seat *wl_seat;
    uint32_t wl_seat_name;

    char *identifier;

    struct pointer  pointer;
    struct keyboard keyboard;

    bool removed;
};

extern struct hashset seats;

#pragma region seat

extern size_t seat_hashset_hash_function(void *ptr);
extern bool seat_hashset_is_equal(void *l, void *r);

extern struct seat *seat_create(struct wl_seat *wl_seat, uint32_t name);
extern void seat_destroy(struct seat *seat);

extern void seat_manage_post_update(struct seat *seat);

#pragma endregion

#pragma region seats

extern void seats_init(void);
extern void seats_cleanup(void);

extern void seats_add(struct wl_seat *wl_seat, uint32_t name);
extern void seats_pend_remove_by_name(uint32_t name);

extern void seats_manage_post_update(void);
extern void seats_manage_removal(void);

#pragma endregion
