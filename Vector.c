#include "Vector.h"

#include <stddef.h>
#include <malloc.h>
#include <memory.h>

int Vector_default_init_size = 8;

void Vector_Zero(struct Vector *vector)
{
    memset(vector, 0, sizeof(struct Vector));
}

bool Vector_Init(struct Vector *vector, int element_size)
{
    vector->array = malloc(element_size * Vector_default_init_size);
    if (vector->array == NULL)
        return false;

    vector->element_size = element_size;
    vector->size = Vector_default_init_size;
    vector->count = 0;

    return true;
}

void Vector_Cleanup(struct Vector *vector)
{
    if (vector->array == NULL)
        return;

    free(vector->array);
    vector->array = NULL;
}

bool Vector_Resize(struct Vector *vector, int new_size)
{
    if (new_size < vector->count)
        return false;

    void *newArrayPtr = realloc(vector->array, vector->element_size * new_size);
    if (newArrayPtr == NULL)
        return false;

    vector->array = newArrayPtr;

    vector->size = new_size;

    return true;
}

bool Vector_Add(struct Vector *vector, void *elem)
{
    if (vector->count == vector->size)
    {
        if (!Vector_Resize(vector, vector->size * 2))
            return false;
    }

    int offset = vector->count * vector->element_size;
    memcpy((int8_t *)vector->array + offset, elem, vector->element_size);

    vector->count++;

    return true;
}

bool Vector_InsertBefore(struct Vector *vector, int index, void *elem)
{
    if (vector->count == vector->size)
    {
        if (!Vector_Resize(vector, vector->size * 2))
            return false;
    }

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

    return true;
}

void Vector_RemoveAt(struct Vector *vector, int index)
{
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

void Vector_Clear(struct Vector *vector)
{
    vector->count = 0;
}

bool Vector_ClearAndFill(struct Vector *vector, void *elem, int count)
{
    Vector_Clear(vector);

    for (int i = 0; i < count; i++)
    {
        if (!Vector_Add(vector, elem))
            return false;
    }

    return true;
}

void *Vector_Index(struct Vector *vector, int index)
{
    if (index >= vector->count || index < 0)
        return NULL;

    int offset = vector->element_size * index;
    return (int8_t *)vector->array + offset;
}

bool Vector_IterInit(struct Vector_Iter *iter, struct Vector *vector)
{
    if (!iter)
        return false;

    if (!vector || !vector->array)
    {
        iter->vector = NULL;
        iter->index = -1;
        return false;
    }

    iter->vector = vector;
    iter->index = 0;

    return true;
}

void *Vector_IterNext(struct Vector_Iter *iter)
{
    if (!iter)
        return NULL;

    // iter info not complete, denied
    if (!iter->vector)
        return NULL;

    void *ret = Vector_Index(iter->vector, iter->index);

    if (ret)
        iter->index++;

    return ret;
}
