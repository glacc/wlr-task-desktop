/*
    Copyright (C) 2026 Glacc

    This file is part of gla-rwm.

    gla-rwm is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    gla-rwm is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with gla-rwm. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#pragma once

#include <stddef.h>

struct LinkedList;

struct LinkedList_Node
{
    struct LinkedList *list;

    struct LinkedList_Node *prev;
    struct LinkedList_Node *next;

    void *elem;
};

struct LinkedList
{
    struct LinkedList_Node *head;
    struct LinkedList_Node *tail;

    size_t count;
};

struct LinkedList_Iter
{
    struct LinkedList *list;
    struct LinkedList_Node *node;
};

extern struct LinkedList_Node *LinkedList_CreateNode(void *elem);
extern void *LinkedList_DestroyNode(struct LinkedList_Node *node);

extern bool LinkedList_InsertElemHead(struct LinkedList *list, void *elem);
extern bool LinkedList_InsertElemTail(struct LinkedList *list, void *elem);
extern bool LinkedList_InsertElemPrev(struct LinkedList_Node *node, void *elem);
extern bool LinkedList_InsertElemNext(struct LinkedList_Node *node, void *elem);

extern bool LinkedList_InsertNodeHead(struct LinkedList *list, struct LinkedList_Node *node);
extern bool LinkedList_InsertNodeTail(struct LinkedList *list, struct LinkedList_Node *node);
extern bool LinkedList_InsertNodeNext(struct LinkedList_Node *dst, struct LinkedList_Node *src);
extern bool LinkedList_InsertNodePrev(struct LinkedList_Node *dst, struct LinkedList_Node *src);

extern struct LinkedList_Node *LinkedList_FindNodeFromHead(struct LinkedList *list, void *elem);
extern struct LinkedList_Node *LinkedList_FindNodeFromTail(struct LinkedList *list, void *elem);
extern struct LinkedList_Node *LinkedList_IndexFromHead(struct LinkedList *list, size_t index);
extern struct LinkedList_Node *LinkedList_IndexFromTail(struct LinkedList *list, size_t index);

extern bool LinkedList_DetachNode(struct LinkedList_Node *node);
extern void *LinkedList_DelHead(struct LinkedList *list);
extern void *LinkedList_DelTail(struct LinkedList *list);
extern void *LinkedList_DelNode(struct LinkedList_Node *node);
extern void LinkedList_DelElemFromHead(struct LinkedList *list, void *elem);
extern void LinkedList_DelElemFromTail(struct LinkedList *list, void *elem);

extern bool LinkedList_IterInit(struct LinkedList_Iter *iter, struct LinkedList *list);
extern struct LinkedList_Node *LinkedList_IterNext(struct LinkedList_Iter *iter);
extern struct LinkedList_Node *LinkedList_IterDel(struct LinkedList_Iter *iter);
