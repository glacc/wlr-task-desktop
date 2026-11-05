#pragma once

#include <stdint.h>
#include <stdbool.h>

extern int Vector_default_init_size;

struct Vector
{
    void *array;
    int element_size;
    int size;
    int count;
};

struct Vector_Iter
{
    struct Vector *vector;
    int index;
};

extern void Vector_Zero(struct Vector *vector);

extern bool Vector_Init(struct Vector *vector, int sizeOfElem);
extern void Vector_Cleanup(struct Vector *vector);

extern bool Vector_Resize(struct Vector *vector, int newSize);

extern bool Vector_Add(struct Vector *vector, void *elem);
extern bool Vector_InsertBefore(struct Vector *vector, int index, void *elem);
extern void Vector_RemoveAt(struct Vector *vector, int index);
extern void Vector_Clear(struct Vector *vector);
extern bool Vector_ClearAndFill(struct Vector *vector, void *elem, int count);

extern void *Vector_Index(struct Vector *vector, int index);

extern bool Vector_IterInit(struct Vector_Iter *iter, struct Vector *vector);
extern void *Vector_IterNext(struct Vector_Iter *iter);
