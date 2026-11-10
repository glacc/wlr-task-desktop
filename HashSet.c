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

#include "HashSet.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

int HashSet_default_init_size = 16;

size_t HashSet_DefaultHash(void *ptr)
{
    // splitmix64 hashing
    size_t x = (size_t)ptr;
    x += 0x9e3779b97f4a7c15;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
    x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
    x = x ^ (x >> 31);
    return x;
}

bool HashSet_Init(struct HashSet *hashset)
{
    if (!hashset)
        return false;

    hashset->size  = HashSet_default_init_size;
    hashset->count = 0;
    hashset->Hash = NULL;
    hashset->IsEqual = NULL;

    if (!(hashset->buckets = calloc(hashset->size, sizeof(*hashset->buckets))))
    {
        fprintf(stderr, "failed to allocate memory for HashSet buckets: %s\n", strerror(errno));
        return false;
    }

    return true;
}

void HashSet_Cleanup(struct HashSet *hashset)
{
    if (!hashset)
        return;

    if (!hashset->buckets)
        return;

    free(hashset->buckets);
    
    hashset->buckets = NULL;
    hashset->size = 0;
    hashset->count = 0;
}

bool HashSet_Add(struct HashSet *hashset, void *elem, void **elem_old)
{
    if (!hashset || !elem)
        return false;

    if (!hashset->buckets)
        return false;

    unsigned int bucket_index;
    
    if (!hashset->Hash)
        bucket_index = HashSet_DefaultHash(elem) % hashset->size;
    else
        bucket_index = hashset->Hash(elem) % hashset->size;
    
    struct LinkedList *bucket = &hashset->buckets[bucket_index];
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *node = NULL;

    // contains same elem
    LinkedList_IterInit(&iter, bucket);

    while ((node = LinkedList_IterNext(&iter)))
    {
        if (!hashset->IsEqual)
        {
            if (node->elem == elem)
                return true;
        }
        else
        {
            if (hashset->IsEqual(node->elem, elem))
            {
                if (elem_old)
                    *elem_old = node->elem;

                node->elem = elem;

                return true;
            }
        }
    }

    // no same
    LinkedList_AddTail(bucket, elem);
    hashset->count++;

    return true;
}

void HashSet_Del(struct HashSet *hashset, void *elem)
{
    if (!hashset || !elem)
        return;

    if (!hashset->buckets)
        return;

    unsigned int bucket_index;
    
    if (!hashset->Hash)
        bucket_index = HashSet_DefaultHash(elem) % hashset->size;
    else
        bucket_index = hashset->Hash(elem) % hashset->size;

    struct LinkedList *bucket = &hashset->buckets[bucket_index];
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *node = NULL;

    LinkedList_IterInit(&iter, bucket);

    while ((node = LinkedList_IterNext(&iter)))
    {
        if (!hashset->IsEqual)
        {
            if (node->elem == elem)
            {
                LinkedList_DelNode(node);
                hashset->count--;
                return;
            }
        }
        else
        {
            if (hashset->IsEqual(node->elem, elem))
            {
                LinkedList_DelNode(node);
                hashset->count--;
                return;
            }
        }
    }
}

void *HashSet_Find(struct HashSet *hashset, void *elem)
{
    if (!hashset || !elem)
        return NULL;

    if (!hashset->buckets)
        return NULL;

    unsigned int bucket_index;
    
    if (!hashset->Hash)
        bucket_index = HashSet_DefaultHash(elem) % hashset->size;
    else
        bucket_index = hashset->Hash(elem) % hashset->size;

    struct LinkedList *bucket = &hashset->buckets[bucket_index];
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *node = NULL;

    LinkedList_IterInit(&iter, bucket);

    while ((node = LinkedList_IterNext(&iter)))
    {
        if (!hashset->IsEqual)
        {
            if (node->elem == elem)
                return node->elem;
        }
        else
        {
            if (hashset->IsEqual(node->elem, elem))
                return node->elem;
        }
    }

    return NULL;
}

bool HashSet_IterInit(struct HashSet_Iter *iter, struct HashSet *hashset)
{
    if (!iter)
        return false;

    iter->bucket_index = 0;

    if (!hashset || !hashset->buckets)
    {
        iter->hashset = NULL;
        iter->node = NULL;
        return false;
    }

    iter->hashset = hashset;

    while (iter->bucket_index < hashset->size)
    {
        iter->node = hashset->buckets[iter->bucket_index].head;

        if (iter->node)
            break;

        iter->bucket_index++;
    }

    return true;
}

void *HashSet_IterNext(struct HashSet_Iter *iter)
{
    if (!iter)
        return NULL;

    // iter info not complete, denied
    if (!iter->hashset)
        return NULL;

    if (!iter->node)
    {
        iter->bucket_index++;

        while (iter->bucket_index < iter->hashset->size)
        {
            iter->node = iter->hashset->buckets[iter->bucket_index].head;

            if (iter->node)
            {
                void *ret = iter->node->elem;

                iter->node = iter->node->next;

                return ret;
            }

            iter->bucket_index++;
        }

        return NULL;
    }

    void *ret = iter->node->elem;
    
    iter->node = iter->node->next;

    return ret;
}

void *HashSet_IterDel(struct HashSet_Iter *iter)
{
    if (!iter)
        return NULL;

    // iter info not complete, denied
    if (!iter->hashset)
        return NULL;

    if (!iter->node)
    {
        iter->bucket_index++;
        
        while (iter->bucket_index < iter->hashset->size)
        {
            iter->node = iter->hashset->buckets[iter->bucket_index].head;

            if (iter->node)
            {
                void *ret = iter->node->elem;
                struct LinkedList_Node *node_to_del = iter->node;

                iter->node = iter->node->next;

                LinkedList_DelNode(node_to_del);

                iter->hashset->count--;

                return ret;
            }

            iter->bucket_index++;
        }

        return NULL;
    }

    void *ret = iter->node->elem;
    struct LinkedList_Node *node_to_del = iter->node;
    
    iter->node = iter->node->next;

    LinkedList_DelNode(node_to_del);

    iter->hashset->count--;

    return ret;
}

bool HashSet_Resize(struct HashSet *hashset, unsigned int size)
{
    if (!hashset)
        return false;

    if (!hashset->buckets)
        return false;

    if (size <= 0)
        return false;

    // allocate new buckets
    struct LinkedList *buckets_new = calloc(size, sizeof(*buckets_new));
    
    // calloc fail
    if (!buckets_new)
        return false;

    // iterate old buckets
    struct HashSet_Iter iter = { };
    void *elem = NULL;
    
    HashSet_IterInit(&iter, hashset);

    while ((elem = HashSet_IterNext(&iter)))
    {
        unsigned int buckets_new_index;
    
        if (!hashset->Hash)
            buckets_new_index = HashSet_DefaultHash(elem) % size;
        else
            buckets_new_index = hashset->Hash(elem) % size;

        struct LinkedList *bucket_new = &buckets_new[buckets_new_index];

        if (!LinkedList_AddHead(bucket_new, elem))
        {
            // failed. clear all lists in new bucket
            for (int i = 0; i < buckets_new_index; i++)
            {
                struct LinkedList_Iter list_iter = { };
                struct LinkedList_Node *node = NULL;
                
                LinkedList_IterInit(&list_iter, bucket_new);

                while ((node = LinkedList_IterDel(&list_iter)));
            }

            free(buckets_new);

            return false;
        }
    }

    // clear old bucket
    for (int i = 0; i < hashset->size; i++)
    {
        struct LinkedList *bucket_old = &hashset->buckets[i];
        struct LinkedList_Iter list_iter = { };
        struct LinkedList_Node *node = NULL;
        
        LinkedList_IterInit(&list_iter, bucket_old);

        while ((node = LinkedList_IterDel(&list_iter)));
    }

    free(hashset->buckets);

    // update fields
    hashset->buckets = buckets_new;
    hashset->size = size;

    return true;
}
