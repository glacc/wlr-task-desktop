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

#include "seat.h"

#include <malloc.h>
#include <string.h>
#include <assert.h>

#include "debug.h"

#include "hashset.h"

struct hashset seats = { };
static struct linked_list seats_to_remove = { };

#pragma region seat

#pragma region seat_listeners

static void seat_capability(
    void *data,
    struct wl_seat *wl_seat,
    uint32_t capabilities
)
{
    struct seat *seat = data;

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
        keyboard_assign_wl_keyboard(&seat->keyboard, wl_seat_get_keyboard(seat->wl_seat));
    else
        keyboard_remove_wl_keyboard(&seat->keyboard);

    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        pointer_assign_wl_pointer(&seat->pointer, wl_seat_get_pointer(seat->wl_seat));
    else
        pointer_remove_wl_pointer(&seat->pointer);
}

static void seat_name(
    void *data,
    struct wl_seat *wl_seat,
    const char *name
)
{
    struct seat *seat = data;
    char *name_old = NULL;

    if (seat->identifier)
        free(seat->identifier);
    
    seat->identifier = strdup(name);

    debug_printf("seat: %s\n", debug_str_nullable(name));
}

static const struct wl_seat_listener seat_listeners =
{
    .capabilities = seat_capability,
    .name         = seat_name,
};

#pragma endregion

#pragma region seat_hashset_functions

size_t seat_hashset_hash_function(void *ptr)
{
    struct seat *seat = ptr;
    return seat->wl_seat_name;
}

bool seat_hashset_is_equal(void *l, void *r)
{
    struct seat *seat_l = l;
    struct seat *seat_r = r;
    return (seat_l->wl_seat_name == seat_r->wl_seat_name);
}

#pragma endregion

struct seat *seat_create(struct wl_seat *wl_seat, uint32_t name)
{
    assert(wl_seat != NULL);

    struct seat *seat = calloc(1, sizeof(*seat));
    assert(seat != NULL);

    seat->wl_seat = wl_seat;
    wl_seat_add_listener(seat->wl_seat, &seat_listeners, seat);

    seat->wl_seat_name = name;

    keyboard_init(&seat->keyboard, seat);
    pointer_init(&seat->pointer, seat);

    return seat;
}

void seat_destroy(struct seat *seat)
{
    assert(seat != NULL);

    if (seat->identifier)
        free(seat->identifier);

    keyboard_remove_wl_keyboard(&seat->keyboard);
    keyboard_cleanup(&seat->keyboard);

    pointer_remove_wl_pointer(&seat->pointer);
    pointer_cleanup(&seat->pointer);
    
    wl_seat_destroy(seat->wl_seat);

    free(seat);
}

void seat_manage_post_update(struct seat *seat)
{
    keyboard_manage_post_update(&seat->keyboard);
    pointer_manage_post_update(&seat->pointer);
}

#pragma endregion

#pragma region seat

void seats_init(void)
{
    hashset_init(&seats);
    seats.hash_function = seat_hashset_hash_function;
    seats.is_equal = seat_hashset_is_equal;
}

void seats_cleanup(void)
{
    seats_manage_removal();
    
    struct hashset_iterator iterator_seats = { };
    struct seat *seat = NULL;
    hashset_iterator_init(&iterator_seats, &seats);
    while ((seat = hashset_iterator_next_deletion(&iterator_seats)))
        seat_destroy(seat);
    hashset_cleanup(&seats);
}

void seats_add(struct wl_seat *wl_seat, uint32_t name)
{
    struct seat *seat = seat_create(wl_seat, name);

    struct seat *overwritten_seat = NULL;
    hashset_insert(&seats, seat, (void **)&overwritten_seat);
    assert(overwritten_seat == NULL);
}

void seats_pend_remove_by_name(uint32_t name)
{
    struct seat seat_to_find = { .wl_seat_name = name };
    struct seat *seat_to_remove = hashset_find(&seats, &seat_to_find);

    if (!seat_to_remove)
        return;

    seat_to_remove->removed = true;

    struct linked_list_node *node_seat_to_remove = linked_list_node_create(seat_to_remove);
    linked_list_insert_node_tail(&seats_to_remove, node_seat_to_remove);

}

void seats_manage_post_update(void)
{
    struct hashset_iterator iterator_seats = { };
    struct seat *seat = NULL;
    hashset_iterator_init(&iterator_seats, &seats);
    while ((seat = hashset_iterator_next(&iterator_seats)))
        seat_manage_post_update(seat);
}

void seats_manage_removal(void)
{
    struct linked_list_iterator iterator_seats = { };
    struct linked_list_node *seat_to_remove_node = NULL;
    struct seat *seat_to_remove = NULL;
    linked_list_iterator_init(&iterator_seats, &seats_to_remove);
    while ((seat_to_remove_node = linked_list_iterator_next_detaching(&iterator_seats)))
    {
        seat_to_remove = seat_to_remove_node->elem;
        hashset_delete(&seats, seat_to_remove);
        seat_destroy(seat_to_remove);
        linked_list_node_destroy(seat_to_remove_node);
    }
}

#pragma endregion
