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

#pragma once

#include <stdint.h>

extern int vector_default_init_size;

struct vector
{
    void *array;
    int element_size;
    int size;
    int count;
};

struct vector_iterator
{
    struct vector *vector;
    int index;
};

extern void vector_init(struct vector *vector, int sizeOfElem);
extern void vector_cleanup(struct vector *vector);

extern void vector_resize(struct vector *vector, int newSize);

extern void *vector_insert(struct vector *vector, void *elem);
extern void *vector_insert_at(struct vector *vector, int index, void *elem);
extern void vector_remove_at(struct vector *vector, int index);
extern void vector_clear(struct vector *vector);
extern void vector_clear_and_fill(struct vector *vector, void *elem, int count);

extern void *vector_get_at(struct vector *vector, int index);

extern void vector_iterator_init(struct vector_iterator *iter, struct vector *vector);
extern void *vector_iterator_next(struct vector_iterator *iter);
