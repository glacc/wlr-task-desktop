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

#include "vector.h"

#include <stddef.h>
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <assert.h>

int vector_default_init_size = 8;

static void inline vector_valid_assert(struct vector *vector)
{
    assert(vector != NULL);
    assert(vector->array != NULL);
}

void vector_init(struct vector *vector, int element_size)
{
    vector->array = malloc(element_size * vector_default_init_size);
    assert(vector->array != NULL);

    vector->element_size = element_size;
    vector->size = vector_default_init_size;
    vector->count = 0;
}

void vector_cleanup(struct vector *vector)
{
    vector_valid_assert(vector);

    free(vector->array);
    vector->array = NULL;
}

void vector_resize(struct vector *vector, int new_size)
{
    assert(new_size >= vector->count);
    
    void *new_array = realloc(vector->array, vector->element_size * new_size);
    assert(new_array != NULL);

    vector->array = new_array;
    vector->size = new_size;
}

void *vector_insert(struct vector *vector, void *elem)
{
    vector_valid_assert(vector);

    if (vector->count == vector->size)
        vector_resize(vector, vector->size * 2);

    int offset = vector->count * vector->element_size;
    int8_t *dst = (int8_t *)vector->array + offset;
    memcpy(dst, elem, vector->element_size);

    vector->count++;

    return dst;
}

void *vector_insert_at(struct vector *vector, int index, void *elem)
{
    vector_valid_assert(vector);

    if (vector->count == vector->size)
        vector_resize(vector, vector->size * 2);

    int8_t *ptr_curr_element = (int8_t *)vector->array + vector->count * vector->element_size;
    int8_t *ptr_prev_element = ptr_curr_element - vector->element_size;

    int move_count = vector->count - index;
    for (int i = 0; i < move_count; i++)
    {
        memcpy(ptr_curr_element, ptr_prev_element, vector->element_size);

        ptr_curr_element = ptr_prev_element;
        ptr_prev_element -= vector->element_size;
    }
    memcpy(ptr_curr_element, elem, vector->element_size);

    vector->count++;

    return ptr_curr_element;
}

void vector_remove_at(struct vector *vector, int index)
{
    vector_valid_assert(vector);

    int8_t *ptr_curr_element = (int8_t *)vector->array + index * vector->element_size;
    int8_t *ptr_next_element = ptr_curr_element + vector->element_size;

    int move_count = vector->count - index - 1;
    for (int i = 0; i < move_count; i++)
    {
        memcpy(ptr_curr_element, ptr_next_element, vector->element_size);

        ptr_curr_element = ptr_next_element;
        ptr_next_element += vector->element_size;
    }

    vector->count--;
}

void vector_clear(struct vector *vector)
{
    vector_valid_assert(vector);

    vector->count = 0;
}

void vector_clear_and_fill(struct vector *vector, void *elem, int count)
{
    vector_valid_assert(vector);

    vector_clear(vector);

    for (int i = 0; i < count; i++)
        vector_insert(vector, elem);
}

void *vector_get_at(struct vector *vector, int index)
{
    vector_valid_assert(vector);

    assert(index >= 0 && index < vector->count);

    int offset = vector->element_size * index;
    return (int8_t *)vector->array + offset;
}

void vector_iterator_init(struct vector_iterator *iterator, struct vector *vector)
{
    assert(iterator != NULL);
    vector_valid_assert(vector);

    iterator->vector = vector;
    iterator->index = 0;
}

void *vector_iterator_next(struct vector_iterator *iterator)
{
    assert(iterator != NULL);
    assert(iterator->vector != NULL);
    vector_valid_assert(iterator->vector);

    if (iterator->index >= iterator->vector->count)
        return NULL;

    void *ret = vector_get_at(iterator->vector, iterator->index);

    iterator->index++;

    return ret;
}
