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

#include <stddef.h>

#include "linked_list.h"

extern int hashset_default_init_size;

// the element should not be NULL
struct hashset
{
    struct linked_list *buckets;
    unsigned int size;
    size_t count;

    // NULL defaults to hash/compare ptr directly
    size_t (*hash_function)(void *obj_ptr);
    bool   (*is_equal)(void *obj_ptr_left, void *obj_ptr_right);
};

struct hashset_iterator
{
    struct hashset *hashset;
    
    unsigned int bucket_index;
    struct linked_list_node *node;
};

// default hash function using splitmix64
extern size_t hashset_default_hash_function(void *ptr);

extern void hashset_init(struct hashset *hashset);
extern void hashset_cleanup(struct hashset *hashset);

// overwriting insertion. elem_old returns NULL or the element been overwritten
extern void hashset_insert(struct hashset *hashset, void *elem, void **elem_old);
extern void hashset_delete(struct hashset *hashset, void *elem);
extern void *hashset_find(struct hashset *hashset, void *elem);

// iterator
// hashset_iterator_next and hashset_iterator_next_deletion should not be used same time in one iteration
extern void hashset_iterator_init(struct hashset_iterator *iterator, struct hashset *hashset);
extern void *hashset_iterator_next(struct hashset_iterator *iterator);
extern void *hashset_iterator_next_deletion(struct hashset_iterator *iterator);

extern void hashset_resize(struct hashset *hashset, unsigned int size);
