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

void Vector_Zero(struct Vector *vector);

bool Vector_Init(struct Vector *vector, int sizeOfElem);
void Vector_Free(struct Vector *vector);

bool Vector_Resize(struct Vector *vector, int newSize);

bool Vector_Add(struct Vector *vector, void *elem);
bool Vector_InsertBefore(struct Vector *vector, int index, void *elem);
void Vector_RemoveAt(struct Vector *vector, int index);
void Vector_Clear(struct Vector *vector);
bool Vector_ClearAndFill(struct Vector *vector, void *elem, int count);

void *Vector_Index(struct Vector *vector, int index);
