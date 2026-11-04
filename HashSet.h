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
