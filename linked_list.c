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

#include "linked_list.h"

#include <malloc.h>
#include <assert.h>

struct linked_list_node *linked_list_node_create(void *elem)
{
    struct linked_list_node *node = calloc(1, sizeof(*node));
    assert(node != NULL);

    node->elem = elem;

    return node;
}

void *linked_list_node_destroy(struct linked_list_node *node)
{
    assert(node != NULL);

    void *elem = node->elem;

    free(node);

    return elem;
}

#pragma region insertion

void linked_list_insert_node_head(struct linked_list *list, struct linked_list_node *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(node->list == NULL);
    node->list = list;

    // not empty
    if (list->head)
    {
        // node <-> old_head
        list->head->prev = node;
        node->next = list->head;
    }
    else
        list->tail = node;

    // head -> node
    list->head = node;

    list->count++;
}

void linked_list_insert_node_tail(struct linked_list *list, struct linked_list_node *node)
{
    assert(list != NULL);
    assert(node != NULL);

    assert(node->list == NULL);
    node->list = list;

    // not empty
    if (list->tail)
    {
        // old_tail <-> node
        list->tail->next = node;
        node->prev = list->tail;
    }
    else
        list->head = node;

    // node <- tail
    list->tail = node;

    list->count++;
}

void linked_list_insert_node_next(struct linked_list_node *dst, struct linked_list_node *src)
{
    assert(dst != NULL);
    assert(src != NULL);

    // node is tail
    if (dst->list->tail == dst)
        return linked_list_insert_node_tail(dst->list, src);

    assert(src->list == NULL);
    src->list = dst->list;

    // node <-> new_next <-> old_next
    src->prev = dst;
    src->next = dst->next;
    dst->next->prev = src;
    dst->next = src;

    dst->list->count++;
}

void linked_list_insert_node_prev(struct linked_list_node *dst, struct linked_list_node *src)
{
    assert(dst != NULL);
    assert(src != NULL);

    // node is head
    if (dst->list->head == dst)
        return linked_list_insert_node_head(dst->list, src);

    assert(src->list == NULL);
    src->list = dst->list;

    // old_prev <-> new_prev <-> node
    src->prev = dst->prev;
    src->next = dst;
    dst->prev->next = src;
    dst->prev = src;

    dst->list->count++;
}

#pragma endregion

#pragma region deletion

void linked_list_node_detach(struct linked_list_node *node)
{
    assert(node != NULL);
    assert(node->list != NULL);

    struct linked_list *list = node->list;

    // deatch head/tail
    if (node == list->head)
    {
        list->head = node->next;

        // if only 1 element
        if (list->head == NULL)
            list->tail = NULL;
        else
        {
            list->head = node->next;
            node->next->prev = NULL;
        }
    }
    else if (node == list->tail)
    {
        list->tail = node->prev;
        
        // if only 1 element
        if (list->tail == NULL)
            list->head = NULL;
        else
        {
            list->tail = node->prev;
            node->prev->next = NULL;
        }
    }
    else
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    node->list = NULL;
    node->prev = NULL;
    node->next = NULL;

    list->count--;
}

#pragma region iterator

void linked_list_iterator_init(struct linked_list_iterator *iter, struct linked_list *list)
{
    assert(iter != NULL);
    assert(list != NULL);

    iter->list = list;
    iter->node = list->head;
}

struct linked_list_node *linked_list_iterator_next(struct linked_list_iterator *iterator)
{
    assert(iterator != NULL);
    assert(iterator->list != NULL);

    if (!iterator->node)
        return NULL;

    struct linked_list_node *node = iterator->node;

    iterator->node = iterator->node->next;

    return node;
}

struct linked_list_node *linked_list_iterator_next_detaching(struct linked_list_iterator *iterator)
{
    assert(iterator != NULL);
    assert(iterator->list != NULL);

    // end reached
    if (!iterator->node)
    {
        iterator->list->head = NULL;
        iterator->list->tail = NULL;
        iterator->list->count = 0;
        return NULL;
    }

    struct linked_list_node *node = iterator->node;

    iterator->node = node->next;

    linked_list_node_detach(node);

    return node;
}

#pragma endregion
