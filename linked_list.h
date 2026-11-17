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

struct linked_list;

struct linked_list_node
{
    struct linked_list *list;

    struct linked_list_node *prev;
    struct linked_list_node *next;

    void *elem;
};

struct linked_list
{
    struct linked_list_node *head;
    struct linked_list_node *tail;

    size_t count;
};

struct linked_list_iterator
{
    struct linked_list *list;
    struct linked_list_node *node;
};

extern struct linked_list_node *linked_list_node_create(void *elem);
extern void *linked_list_node_destroy(struct linked_list_node *node);

// node insertion
// inserting a node already belongs to a linked list will fail
extern void linked_list_insert_node_head(struct linked_list *list, struct linked_list_node *node);
extern void linked_list_insert_node_tail(struct linked_list *list, struct linked_list_node *node);
extern void linked_list_insert_node_next(struct linked_list_node *dst, struct linked_list_node *src);
extern void linked_list_insert_node_prev(struct linked_list_node *dst, struct linked_list_node *src);

// deletion
extern void linked_list_node_detach(struct linked_list_node *node);

// iterator
// linked_list_iterator_next and linked_list_iterator_next_detaching should not be used same time in one iteration
extern void linked_list_iterator_init(struct linked_list_iterator *iterator, struct linked_list *list);
extern struct linked_list_node *linked_list_iterator_next(struct linked_list_iterator *iterator);
extern struct linked_list_node *linked_list_iterator_next_detaching(struct linked_list_iterator *iterator);
