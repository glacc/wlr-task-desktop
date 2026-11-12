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

extern bool Vector_Init(struct Vector *vector, int sizeOfElem);
extern void Vector_Cleanup(struct Vector *vector);

extern bool Vector_Resize(struct Vector *vector, int newSize);

extern void *Vector_Add(struct Vector *vector, void *elem);
extern void *Vector_InsertBefore(struct Vector *vector, int index, void *elem);
extern void Vector_RemoveAt(struct Vector *vector, int index);
extern void Vector_Clear(struct Vector *vector);
extern bool Vector_ClearAndFill(struct Vector *vector, void *elem, int count);

extern void *Vector_Index(struct Vector *vector, int index);

extern bool Vector_IterInit(struct Vector_Iter *iter, struct Vector *vector);
extern void *Vector_IterNext(struct Vector_Iter *iter);
