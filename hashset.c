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

#include "hashset.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

int hashset_default_init_size = 16;

static inline void hashset_valid_assert(struct hashset *hashset)
{
    assert(hashset != NULL);
    assert(hashset->buckets != NULL);
}

size_t hashset_default_hash_function(void *ptr)
{
    // splitmix64 hashing
    size_t x = (size_t)ptr;
    x += 0x9e3779b97f4a7c15;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
    x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
    x = x ^ (x >> 31);
    return x;
}

void hashset_init(struct hashset *hashset)
{
    assert(hashset != NULL);

    hashset->size  = hashset_default_init_size;
    hashset->count = 0;
    hashset->hash_function = NULL;
    hashset->is_equal = NULL;

    hashset->buckets = calloc(hashset->size, sizeof(*hashset->buckets));
    assert(hashset->buckets != NULL);
}

void hashset_cleanup(struct hashset *hashset)
{
    hashset_valid_assert(hashset);

    free(hashset->buckets);
    
    hashset->buckets = NULL;
    hashset->size = 0;
    hashset->count = 0;
}

void hashset_insert(struct hashset *hashset, void *elem, void **elem_old)
{
    hashset_valid_assert(hashset);
    assert(elem != NULL);
    
    unsigned int bucket_index;
    
    if (!hashset->hash_function)
        bucket_index = hashset_default_hash_function(elem) % hashset->size;
    else
        bucket_index = hashset->hash_function(elem) % hashset->size;
    
    struct linked_list *bucket = &hashset->buckets[bucket_index];
    struct linked_list_iterator iterator = { };
    struct linked_list_node *node = NULL;

    // contains same element
    linked_list_iterator_init(&iterator, bucket);

    while ((node = linked_list_iterator_next(&iterator)))
    {
        if (!hashset->is_equal)
        {
            if (elem == node->elem)
            {
                if (elem_old)
                    *elem_old = node->elem;

                node->elem = elem;

                return;
            }
        }
        else
        {
            if (hashset->is_equal(node->elem, elem))
            {
                if (elem_old)
                    *elem_old = node->elem;

                node->elem = elem;

                return;
            }
        }
    }

    // not same
    struct linked_list_node *node_new = linked_list_node_create(elem);
    linked_list_insert_node_tail(bucket, node_new);

    hashset->count++;
}

void hashset_delete(struct hashset *hashset, void *elem)
{
    hashset_valid_assert(hashset);
    assert(elem != NULL);

    unsigned int bucket_index;
    
    if (!hashset->hash_function)
        bucket_index = hashset_default_hash_function(elem) % hashset->size;
    else
        bucket_index = hashset->hash_function(elem) % hashset->size;

    struct linked_list *bucket = &hashset->buckets[bucket_index];
    struct linked_list_iterator iter = { };
    struct linked_list_node *node = NULL;

    linked_list_iterator_init(&iter, bucket);

    while ((node = linked_list_iterator_next(&iter)))
    {
        if (!hashset->is_equal)
        {
            if (node->elem != elem)
                continue;

            linked_list_node_detach(node);
            linked_list_node_destroy(node);

            hashset->count--;

            return;
        }
        else
        {
            if (!hashset->is_equal(node->elem, elem))
                continue;

            linked_list_node_detach(node);
            linked_list_node_destroy(node);

            hashset->count--;

            return;
        }
    }
}

void *hashset_find(struct hashset *hashset, void *elem)
{
    assert(elem != NULL);

    unsigned int bucket_index;
    
    if (!hashset->hash_function)
        bucket_index = hashset_default_hash_function(elem) % hashset->size;
    else
        bucket_index = hashset->hash_function(elem) % hashset->size;

    struct linked_list *bucket = &hashset->buckets[bucket_index];
    struct linked_list_iterator iterator = { };
    struct linked_list_node *node = NULL;

    linked_list_iterator_init(&iterator, bucket);

    while ((node = linked_list_iterator_next(&iterator)))
    {
        if (!hashset->is_equal)
        {
            if (node->elem == elem)
                return node->elem;
        }
        else
        {
            if (hashset->is_equal(node->elem, elem))
                return node->elem;
        }
    }

    return NULL;
}

void hashset_iterator_init(struct hashset_iterator *iterator, struct hashset *hashset)
{
    assert(iterator != NULL);
    hashset_valid_assert(hashset);

    iterator->bucket_index = 0;

    iterator->hashset = hashset;

    while (iterator->bucket_index < hashset->size)
    {
        iterator->node = hashset->buckets[iterator->bucket_index].head;

        if (iterator->node)
            break;

        iterator->bucket_index++;
    }
}

void *hashset_iterator_next(struct hashset_iterator *iterator)
{
    assert(iterator->hashset != NULL);
    hashset_valid_assert(iterator->hashset);

    if (!iterator->node)
    {
        iterator->bucket_index++;

        while (iterator->bucket_index < iterator->hashset->size)
        {
            iterator->node = iterator->hashset->buckets[iterator->bucket_index].head;

            if (iterator->node)
            {
                void *ret = iterator->node->elem;

                iterator->node = iterator->node->next;

                return ret;
            }

            iterator->bucket_index++;
        }

        return NULL;
    }

    void *ret = iterator->node->elem;
    
    iterator->node = iterator->node->next;

    return ret;
}

void *hashset_iterator_next_deletion(struct hashset_iterator *iterator)
{
    assert(iterator->hashset != NULL);
    hashset_valid_assert(iterator->hashset);

    // iter info not complete, denied
    if (!iterator->hashset)
        return NULL;

    if (!iterator->node)
    {
        iterator->bucket_index++;
        
        while (iterator->bucket_index < iterator->hashset->size)
        {
            iterator->node = iterator->hashset->buckets[iterator->bucket_index].head;

            if (iterator->node)
            {
                void *ret = iterator->node->elem;
                struct linked_list_node *node_to_del = iterator->node;

                iterator->node = iterator->node->next;

                linked_list_node_detach(node_to_del);
                linked_list_node_destroy(node_to_del);

                iterator->hashset->count--;

                return ret;
            }

            iterator->bucket_index++;
        }

        return NULL;
    }

    void *ret = iterator->node->elem;
    struct linked_list_node *node_to_del = iterator->node;
    
    iterator->node = iterator->node->next;

    linked_list_node_detach(node_to_del);
    linked_list_node_destroy(node_to_del);

    iterator->hashset->count--;

    return ret;
}

void hashset_resize(struct hashset *hashset, unsigned int size)
{
    hashset_valid_assert(hashset);
    assert(size > 0);

    // allocate new buckets
    struct linked_list *buckets_new = calloc(size, sizeof(*buckets_new));
    assert(buckets_new != NULL);

    // iterate old buckets
    struct hashset_iterator iter = { };
    void *elem = NULL;
    
    hashset_iterator_init(&iter, hashset);

    while ((elem = hashset_iterator_next(&iter)))
    {
        unsigned int buckets_new_index;
    
        if (!hashset->hash_function)
            buckets_new_index = hashset_default_hash_function(elem) % size;
        else
            buckets_new_index = hashset->hash_function(elem) % size;

        struct linked_list *bucket_new = &buckets_new[buckets_new_index];

        struct linked_list_node *node_new = linked_list_node_create(elem);
        linked_list_insert_node_tail(bucket_new, node_new);
    }

    // clear old bucket
    for (int i = 0; i < hashset->size; i++)
    {
        struct linked_list *bucket_old = &hashset->buckets[i];
        struct linked_list_iterator list_iter = { };
        struct linked_list_node *node = NULL;
        
        linked_list_iterator_init(&list_iter, bucket_old);

        while ((node = linked_list_iterator_next_detaching(&list_iter)))
            linked_list_node_destroy(node);
    }

    free(hashset->buckets);

    // update fields
    hashset->buckets = buckets_new;
    hashset->size = size;
}
