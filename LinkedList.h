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

extern bool LinkedList_AddHead(struct LinkedList *list, void *elem);
extern bool LinkedList_AddTail(struct LinkedList *list, void *elem);
extern bool LinkedList_AddPrev(struct LinkedList_Node *node, void *elem);
extern bool LinkedList_AddNext(struct LinkedList_Node *node, void *elem);

extern struct LinkedList_Node *LinkedList_FindNodeFromHead(struct LinkedList *list, void *elem);
extern struct LinkedList_Node *LinkedList_FindNodeFromTail(struct LinkedList *list, void *elem);
extern struct LinkedList_Node *LinkedList_IndexFromHead(struct LinkedList *list, size_t index);
extern struct LinkedList_Node *LinkedList_IndexFromTail(struct LinkedList *list, size_t index);

extern void *LinkedList_DelHead(struct LinkedList *list);
extern void *LinkedList_DelTail(struct LinkedList *list);
extern void *LinkedList_DelNode(struct LinkedList_Node *node);
extern void LinkedList_DelElemFromHead(struct LinkedList *list, void *elem);
extern void LinkedList_DelElemFromTail(struct LinkedList *list, void *elem);

extern void LinkedList_IterInit(struct LinkedList_Iter *iter, struct LinkedList *list);
extern struct LinkedList_Node *LinkedList_IterNext(struct LinkedList_Iter *iter);
extern void *LinkedList_IterDel(struct LinkedList_Iter *iter);
