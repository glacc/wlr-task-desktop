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
#include <stdbool.h>

#include "LinkedList.h"

extern int HashSet_default_init_size;

struct HashSet
{
    struct LinkedList *buckets;
    unsigned int size;
    size_t count;

    // NULL defaults to hash/compare ptr directly
    size_t (*Hash)(void *obj_ptr);
    bool   (*IsEqual)(void *obj_ptr_left, void *obj_ptr_right);
};

struct HashSet_Iter
{
    struct HashSet *hashset;
    
    unsigned int bucket_index;
    struct LinkedList_Node *node;
};

extern size_t HashSet_DefaultHash(void *ptr);

extern bool HashSet_Init(struct HashSet *hashset);
extern void HashSet_Cleanup(struct HashSet *hashset);

extern bool HashSet_Add(struct HashSet *hashset, void *elem, void **elem_old);
extern void HashSet_Del(struct HashSet *hashset, void *elem);
extern void *HashSet_Find(struct HashSet *hashset, void *elem);

extern bool HashSet_IterInit(struct HashSet_Iter *iter, struct HashSet *hashset);
extern void *HashSet_IterNext(struct HashSet_Iter *iter);
extern void *HashSet_IterDel(struct HashSet_Iter *iter);

extern bool HashSet_Resize(struct HashSet *hashset, unsigned int size);
