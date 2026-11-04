#include "LinkedList.h"

#include <malloc.h>

#pragma region Insertion

bool LinkedList_AddHead(struct LinkedList *list, void *elem)
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

bool LinkedList_AddTail(struct LinkedList *list, void *elem)
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

bool LinkedList_AddNext(struct LinkedList_Node *node, void *elem)
{
    if (!node || !elem)
        return false;

    // no list assigned
    if (!node->list)
        return false;

    // node is tail
    if (node->list->tail == node)
        return LinkedList_AddTail(node->list, elem);

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

bool LinkedList_AddPrev(struct LinkedList_Node *node, void *elem)
{
    if (!node || !elem)
        return false;

    // no list assigned
    if (!node->list)
        return false;

    // node is head
    if (node->list->head == node)
        return LinkedList_AddHead(node->list, elem);

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
    size_t idx_cur = list->count - 1;

    while (node)
    {
        // reached
        if (idx_cur == index)
            return node;

        node = node->prev;
        idx_cur--;
    }

    // unlikely
    return NULL;
}

#pragma endregion

#pragma region Deletion

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
    if (!node)
        return NULL;

    // no list assigned
    if (!node->list)
        return NULL;

    // is head
    if (node->list->head == node)
        return LinkedList_DelHead(node->list);

    // is tail
    if (node->list->tail == node)
        return LinkedList_DelTail(node->list);

    // general
    // prev <-> next
    void *ret = node->elem;

    node->prev->next = node->next;
    node->next->prev = node->prev;

    node->list->count--;

    free(node);

    return ret;
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

void LinkedList_IterInit(struct LinkedList_Iter *iter, struct LinkedList *list)
{
    if (!iter)
        return;

    if (!list)
    {
        iter->list = NULL;
        iter->node = NULL;
        return;
    }

    iter->list = list;
    iter->node = list->head;
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

void *LinkedList_IterDel(struct LinkedList_Iter *iter)
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
    void *ret = node->elem;

    iter->node = node->next;
    free(node);

    return ret;
}

#pragma endregion
