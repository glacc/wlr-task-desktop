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

#include "LinkedList.h"

#include <malloc.h>

struct LinkedList_Node *LinkedList_CreateNode(void *elem)
{
    struct LinkedList_Node *node = calloc(1, sizeof(*node));
    
    if (!node)
        return NULL;

    node->elem = elem;

    return node;
}

void *LinkedList_DestroyNode(struct LinkedList_Node *node)
{
    if (!node)
        return NULL;

    LinkedList_DetachNode(node);

    void *elem = node->elem;

    free(node);

    return elem;
}

#pragma region Insertion

bool LinkedList_InsertElemHead(struct LinkedList *list, void *elem)
{
    if (!list || !elem)
        return false;

    struct LinkedList_Node *node = calloc(1, sizeof(*node));
    
    // calloc fail
    if (!node)
        return false;

    node->list = list;
    node->elem = elem;

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

    return true;
}

bool LinkedList_InsertElemTail(struct LinkedList *list, void *elem)
{
    if (!list || !elem)
        return false;

    struct LinkedList_Node *node = calloc(1, sizeof(*node));

    // calloc fail
    if (!node)
        return false;

    node->list = list;
    node->elem = elem;

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

    return true;
}

bool LinkedList_InsertElemNext(struct LinkedList_Node *node, void *elem)
{
    if (!node || !elem)
        return false;

    // no list assigned
    if (!node->list)
        return false;

    // node is tail
    if (node->list->tail == node)
        return LinkedList_InsertElemTail(node->list, elem);

    // general
    struct LinkedList_Node *new_next = calloc(1, sizeof(*node));
    
    // calloc fail
    if (!new_next)
        return false;

    new_next->list = node->list;
    new_next->elem = elem;

    // node <-> new_next <-> old_next
    new_next->prev = node;
    new_next->next = node->next;
    node->next->prev = new_next;
    node->next = new_next;

    node->list->count++;

    return true;
}

bool LinkedList_InsertElemPrev(struct LinkedList_Node *node, void *elem)
{
    if (!node || !elem)
        return false;

    // no list assigned
    if (!node->list)
        return false;

    // node is head
    if (node->list->head == node)
        return LinkedList_InsertElemHead(node->list, elem);

    // general
    struct LinkedList_Node *new_prev = calloc(1, sizeof(*node));
    
    // calloc fail
    if (!new_prev)
        return false;

    new_prev->list = node->list;
    new_prev->elem = elem;

    // old_prev <-> new_prev <-> node
    new_prev->prev = node->prev;
    new_prev->next = node;
    node->prev->next = new_prev;
    node->prev = new_prev;

    node->list->count++;

    return true;
}

bool LinkedList_InsertNodeHead(struct LinkedList *list, struct LinkedList_Node *node)
{
    if (!list || !node)
        return false;

    if (node->list)
        return false;

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

    return true;
}

bool LinkedList_InsertNodeTail(struct LinkedList *list, struct LinkedList_Node *node)
{
    if (!list || !node)
        return false;

    if (node->list)
        return false;

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

    return true;
}

bool LinkedList_InsertNodeNext(struct LinkedList_Node *dst, struct LinkedList_Node *src)
{
    if (!dst || !src)
        return false;

    if (src->list)
        return false;

    // no list assigned
    if (!dst->list)
        return false;

    // node is tail
    if (dst->list->tail == dst)
        return LinkedList_InsertNodeTail(dst->list, src);

    src->list = dst->list;

    // node <-> new_next <-> old_next
    src->prev = dst;
    src->next = dst->next;
    dst->next->prev = src;
    dst->next = src;

    dst->list->count++;

    return true;
}

bool LinkedList_InsertNodePrev(struct LinkedList_Node *dst, struct LinkedList_Node *src)
{
    if (!dst || !src)
        return false;

    if (src->list)
        return false;

    // no list assigned
    if (!dst->list)
        return false;

    // node is head
    if (dst->list->head == dst)
        return LinkedList_InsertNodeHead(dst->list, src);

    src->list = dst->list;

    // old_prev <-> new_prev <-> node
    src->prev = dst->prev;
    src->next = dst;
    dst->prev->next = src;
    dst->prev = src;

    dst->list->count++;

    return true;
}

#pragma endregion

#pragma region Search

struct LinkedList_Node *LinkedList_FindNodeFromHead(struct LinkedList *list, void *elem)
{
    if (!list || !elem)
        return NULL;

    struct LinkedList_Node *node = list->head;
    
    while (node)
    {
        // found
        if (node->elem == elem)
            break;

        node = node->next;
    }

    return node;
}

struct LinkedList_Node *LinkedList_FindNodeFromTail(struct LinkedList *list, void *elem)
{
    if (!list || !elem)
        return NULL;

    struct LinkedList_Node *node = list->tail;
    
    while (node)
    {
        // found
        if (node->elem == elem)
            break;

        node = node->prev;
    }

    return node;
}

struct LinkedList_Node *LinkedList_IndexFromHead(struct LinkedList *list, size_t index)
{
    if (!list)
        return NULL;

    // out of range
    if (index >= list->count)
        return NULL;

    struct LinkedList_Node *node = list->head;
    size_t idx_cur = 0;

    while (node)
    {
        // reached
        if (idx_cur == index)
            return node;

        node = node->next;
        idx_cur++;
    }

    // unlikely
    return NULL;
}

struct LinkedList_Node *LinkedList_IndexFromTail(struct LinkedList *list, size_t index)
{
    if (!list)
        return NULL;

    // out of range
    if (index >= list->count)
        return NULL;

    struct LinkedList_Node *node = list->tail;
    size_t idx_cur = 0;

    while (node)
    {
        // reached
        if (idx_cur == index)
            return node;

        node = node->prev;
        idx_cur++;
    }

    // unlikely
    return NULL;
}

#pragma endregion

#pragma region Deletion

bool LinkedList_DetachNode(struct LinkedList_Node *node)
{
    if (!node || !node->list)
        return false;

    struct LinkedList *list = node->list;

    // deatch head/tail
    if (node == list->head)
    {
        list->head = node->next;

        if (list->head == NULL)
            list->tail = NULL;
    }
    else if (node == list->tail)
    {
        list->tail = node->prev;
        
        if (list->tail == NULL)
            list->head = NULL;
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

    return true;
}

void *LinkedList_DelHead(struct LinkedList *list)
{
    if (!list)
        return NULL;

    // empty
    if (!list->head)
        return NULL;

    struct LinkedList_Node *head = list->head;
    
    // head -> (head->next)
    list->head = head->next;

    if (head->next)
        head->next->prev = NULL;
    else
        list->tail = NULL;

    void *elem = head->elem;

    free(head);

    list->count--;

    return elem;
}

void *LinkedList_DelTail(struct LinkedList *list)
{
    if (!list)
        return NULL;

    // empty
    if (!list->tail)
        return NULL;

    struct LinkedList_Node *tail = list->tail;

    // (tail->prev) <- tail
    list->tail = tail->prev;

    if (tail->prev)
        tail->prev->next = NULL;
    else
        list->head = NULL;

    void *elem = tail->elem;

    free(tail);

    list->count--;

    return elem;
}

void *LinkedList_DelNode(struct LinkedList_Node *node)
{
    return LinkedList_DestroyNode(node);
}

void LinkedList_DelElemFromHead(struct LinkedList *list, void *elem)
{
    if (!list || !elem)
        return;

    // empty
    if (!list->head)
        return;

    // is head
    if (list->head->elem == elem)
    {
        LinkedList_DelHead(list);
        return;
    }

    struct LinkedList_Node *node = LinkedList_FindNodeFromHead(list, elem);

    // not found
    if (!node)
        return;

    LinkedList_DelNode(node);
}

void LinkedList_DelElemFromTail(struct LinkedList *list, void *elem)
{
    if (!list || !elem)
        return;

    // empty
    if (!list->tail)
        return;

    // is tail
    if (list->tail->elem == elem)
    {
        LinkedList_DelTail(list);
        return;
    }

    struct LinkedList_Node *node = LinkedList_FindNodeFromHead(list, elem);

    // not found
    if (!node)
        return;

    // is head
    if (list->head == node)
    {
        LinkedList_DelHead(list);
        return;
    }

    LinkedList_DelNode(node);
}

#pragma endregion

#pragma region Iterator

bool LinkedList_IterInit(struct LinkedList_Iter *iter, struct LinkedList *list)
{
    if (!iter)
        return false;

    if (!list)
    {
        iter->list = NULL;
        iter->node = NULL;
        return false;
    }

    iter->list = list;
    iter->node = list->head;

    return true;
}

struct LinkedList_Node *LinkedList_IterNext(struct LinkedList_Iter *iter)
{
    if (!iter)
        return NULL;

    if (!iter->node)
        return NULL;

    struct LinkedList_Node *node = iter->node;

    iter->node = iter->node->next;

    return node;
}

struct LinkedList_Node *LinkedList_IterDel(struct LinkedList_Iter *iter)
{
    if (!iter)
        return NULL;

    // iter info not complete, denied
    if (!iter->list)
        return NULL;

    // end reached
    if (!iter->node)
    {
        iter->list->head = NULL;
        iter->list->tail = NULL;
        iter->list->count = 0;
        return NULL;
    }

    struct LinkedList_Node *node = iter->node;

    iter->node = node->next;

    LinkedList_DetachNode(node);

    return node;
}

#pragma endregion
