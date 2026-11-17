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

#include "double_buffer_surface.h"

#include <malloc.h>
#include <assert.h>

#include "main.h"

#include "seat.h"

#include "SWRender/SWRender.h"

static void double_buffer_surface_copy_rect_from_swrender_buf_to_buffer(struct buffer *dst, struct double_buffer_surface *src, struct buffer_rect *rect)
{
    int32_t x1 = rect->x;
    int32_t x2 = x1 + rect->w;
    int32_t y1 = rect->y;
    int32_t y2 = y1 + rect->h;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 <= 0) return;
    if (y2 <= 0) return;
    if (x2 > src->swrender_buffer.w) x2 = src->swrender_buffer.w;
    if (y2 > src->swrender_buffer.h) y2 = src->swrender_buffer.h;
    if (x2 > dst->w) x2 = dst->w;
    if (y2 > dst->h) y2 = dst->h;
    if (x1 >= x2) return;
    if (y1 >= y2) return;

    SWRenderColor       *ptr_row_start_src = SWRender_GetPointerByPosition(&src->swrender_buffer, x1, y1);
    struct buffer_pixel *ptr_row_start_dst = dst->pixels + (y1 * dst->w) + x1;

    while (y1 < y2)
    {
        SWRenderColor       *ptr_src = ptr_row_start_src;
        struct buffer_pixel *ptr_dst = ptr_row_start_dst;

        for (int x = x1; x < x2; x++)
        {
            float a_norm = ptr_src->a * (1.0F / 255.0F);
            ptr_dst->r = (uint8_t)(((float)ptr_src->r * a_norm) + 0.5F);
            ptr_dst->g = (uint8_t)(((float)ptr_src->g * a_norm) + 0.5F);
            ptr_dst->b = (uint8_t)(((float)ptr_src->b * a_norm) + 0.5F);

            ptr_dst->a = ptr_src->a;

            ptr_src++;
            ptr_dst++;
        }

        ptr_row_start_src = SWRender_GetPointerNextLine(&src->swrender_buffer, ptr_row_start_src);
        ptr_row_start_dst += dst->w;

        y1++;
    }
}

void double_buffer_surface_init(struct double_buffer_surface *surface)
{
    assert(surface != NULL);

    // surface creation
    surface->wl_surface = wl_compositor_create_surface(wl_compositor);
    wl_surface_set_user_data(surface->wl_surface, surface);

    for (int i = 0; i < 2; i++)
        vector_init(&surface->damage_rects[i], sizeof(struct buffer_rect));

    surface->commit_count = 0;

    surface->valid = false;

    // keyboard state init
    hashset_init(&surface->keyboard_states);
    surface->keyboard_states.hash_function = keyboard_state_hash_function;
    surface->keyboard_states.is_equal = keyboard_state_is_equal;

    // pointer state init
    hashset_init(&surface->pointer_states);
    surface->pointer_states.hash_function = pointer_state_hash_function;
    surface->pointer_states.is_equal = pointer_state_is_equal;
}

void double_buffer_surface_cleanup(struct double_buffer_surface *surface)
{
    assert(surface != NULL);

    wl_surface_destroy(surface->wl_surface);

    for (int i = 0; i < 2; i++)
    {
        if (surface->valid)
            buffer_cleanup(&surface->buffer[i]);

        vector_cleanup(&surface->damage_rects[i]);
    }
    if (surface->valid)
        SWRender_BufferFree(&surface->swrender_buffer);

    surface->valid = false;

    // keyboard states
    struct hashset_iterator iterator_keyboard_states = { };
    struct keyboard_state *keyboard_state = NULL;
    hashset_iterator_init(&iterator_keyboard_states, &surface->keyboard_states);
    while ((keyboard_state = hashset_iterator_next_deletion(&iterator_keyboard_states)))
    {
        struct seat *seat = keyboard_state->seat;
        assert(seat != NULL);

        if (seat->keyboard.surface_curr == surface)
            seat->keyboard.surface_curr = NULL;

        keyboard_state_destroy(keyboard_state);
        hashset_delete(&seat->keyboard.surface_visited, surface);
    }
    hashset_cleanup(&surface->keyboard_states);

    // pointer states
    struct hashset_iterator iterator_pointer_states = { };
    struct pointer_state *pointer_state = NULL;
    hashset_iterator_init(&iterator_pointer_states, &surface->pointer_states);
    while ((pointer_state = hashset_iterator_next_deletion(&iterator_pointer_states)))
    {
        struct seat *seat = pointer_state->seat;
        assert(seat != NULL);

        if (seat->pointer.surface_curr == surface)
            seat->pointer.surface_curr = NULL;

        pointer_state_destroy(pointer_state);
        hashset_delete(&seat->pointer.surface_visited, surface);
    }
    hashset_cleanup(&surface->pointer_states);
}

#pragma region wl_surface_related

void double_buffer_surface_set_size(struct double_buffer_surface *surface, int32_t w, int32_t h)
{
    assert(surface != NULL);

    if (surface->valid)
    {
        if (surface->w == w && surface->h == h)
            return;
        
        for (int i = 0; i < 2; i++)
        {
            buffer_cleanup(&surface->buffer[i]);
            vector_clear(&surface->damage_rects[i]);
        }
        SWRender_BufferFree(&surface->swrender_buffer);
    }
    surface->valid = false;

    // return if invalid
    if (!(w > 0 && h > 0))
        return;

    for (int i = 0; i < 2; i++)
        buffer_init(&surface->buffer[i], w, h);

    surface->w = surface->swrender_buffer.w = w;
    surface->h = surface->swrender_buffer.h = h;
    SWRender_BufferAlloc(&surface->swrender_buffer);

    surface->commit_count = 0;

    surface->valid = true;
}

void double_buffer_surface_damage(struct double_buffer_surface *surface, const struct buffer_rect *rect)
{
    assert(surface != NULL);

    struct vector *vec_damage_rects = &surface->damage_rects[surface->commit_count & 1];

    if (rect)
    {
        struct buffer_rect rect_final = *rect;

        // check is invalid or not inside?
        if (rect_final.w <= 0 || rect_final.h <= 0)
            return;
        if (rect_final.x >= surface->w || rect_final.y >= surface->h)
            return;
        if ((rect_final.x + rect_final.w) <= 0 || (rect_final.y + rect_final.h) <= 0)
            return;

        // crop left & top
        if (rect_final.x < 0)
        {
            rect_final.w += rect_final.x;
            rect_final.x = 0;
        }
        if (rect_final.y < 0)
        {
            rect_final.h += rect_final.y;
            rect_final.y = 0;
        }
        
        // crop right & bottom
        int32_t x_right  = rect_final.x + rect_final.w;
        int32_t y_bottom = rect_final.y + rect_final.h;

        if (x_right  > surface->w)
            rect_final.w -= x_right  - surface->w;
        if (y_bottom > surface->h)
            rect_final.h -= y_bottom - surface->h;

        vector_insert(vec_damage_rects, &rect_final);
    }
    else
        vector_insert(vec_damage_rects, &(struct buffer_rect){ 0, 0, surface->w, surface->h });
}

bool double_buffer_surface_is_any_buffer_available(struct double_buffer_surface *surface)
{
    assert(surface != NULL);

    bool has_available_surface = false;

    for (int i = 0; i < 2; i++)
    {
        if (!surface->buffer[i].valid)
            return false;
        if (surface->buffer[i].available)
            has_available_surface = true;
    }
    return has_available_surface;
}

bool double_buffer_surface_try_commit(struct double_buffer_surface *surface)
{
    assert(surface != NULL);

    unsigned int buffer_curr_index = surface->commit_count & 1;
    struct buffer *buffer_curr = &surface->buffer[buffer_curr_index];

    if (!buffer_curr->available)
        return false;

    // attach current buffer
    wl_surface_attach(surface->wl_surface, buffer_curr->wl_buffer, 0, 0);

    unsigned int buffer_prev_index = buffer_curr_index ^ 1;
    struct buffer *buffer_prev = &surface->buffer[buffer_prev_index];
    
    // send damage to wayland
    struct buffer_rect *buffer_rect;
    struct vector_iterator iter = { };

    // prev buffer
    vector_iterator_init(&iter, &surface->damage_rects[buffer_prev_index]);
    while ((buffer_rect = vector_iterator_next(&iter)))
    {
        double_buffer_surface_copy_rect_from_swrender_buf_to_buffer(buffer_curr, surface, buffer_rect);
        wl_surface_damage_buffer(surface->wl_surface, buffer_rect->x, buffer_rect->y, buffer_rect->w, buffer_rect->h);
    }
    vector_clear(&surface->damage_rects[buffer_prev_index]);

    // curr buffer
    vector_iterator_init(&iter, &surface->damage_rects[buffer_curr_index]);
    while ((buffer_rect = vector_iterator_next(&iter)))
    {
        double_buffer_surface_copy_rect_from_swrender_buf_to_buffer(buffer_curr, surface, buffer_rect);
        wl_surface_damage_buffer(surface->wl_surface, buffer_rect->x, buffer_rect->y, buffer_rect->w, buffer_rect->h);
    }

    wl_surface_commit(surface->wl_surface);
    buffer_curr->available = false;

    surface->commit_count++;

    return true;
}

#pragma endregion

#pragma region keyboard_state

struct keyboard_state *double_buffer_surface_get_keyboard_state(struct double_buffer_surface *surface, struct seat *seat)
{
    assert(surface != NULL && seat != NULL);

    struct keyboard_state keyboard_state_to_find = { .seat = seat };
    struct keyboard_state *keyboard_state = hashset_find(&surface->keyboard_states, &keyboard_state_to_find);

    return keyboard_state;
}

struct keyboard_state *double_buffer_surface_create_or_get_keyboard_state(struct double_buffer_surface *surface, struct seat *seat)
{
    assert(surface != NULL && seat != NULL);

    struct keyboard_state *keyboard_state = double_buffer_surface_get_keyboard_state(surface, seat);

    if (keyboard_state)
        return keyboard_state;

    keyboard_state = calloc(1, sizeof(*keyboard_state));
    keyboard_state_init(keyboard_state, seat);
    struct keyboard_state *overwritten_keyboard_state = NULL;
    hashset_insert(&surface->keyboard_states, keyboard_state, (void **)&overwritten_keyboard_state);
    assert(overwritten_keyboard_state == NULL);

    return keyboard_state;
}

void double_buffer_surface_remove_keyboard_state_by_seat(struct double_buffer_surface *surface, struct seat *seat)
{
    assert(surface != NULL && seat != NULL);

    struct keyboard_state keyboard_state_to_find = { .seat = seat };
    struct keyboard_state *keyboard_state = hashset_find(&surface->keyboard_states, &keyboard_state_to_find);
    
    if (!keyboard_state)
        return;

    hashset_delete(&surface->keyboard_states, keyboard_state);
    keyboard_state_destroy(keyboard_state);
}

#pragma endregion

#pragma region pointer_state

struct pointer_state *double_buffer_surface_get_pointer_state(struct double_buffer_surface *surface, struct seat *seat)
{
    assert(surface != NULL && seat != NULL);

    struct pointer_state pointer_state_to_find = { .seat = seat };
    struct pointer_state *pointer_state = hashset_find(&surface->pointer_states, &pointer_state_to_find);

    return pointer_state;
}

struct pointer_state *double_buffer_surface_create_or_get_pointer_state(struct double_buffer_surface *surface, struct seat *seat)
{
    assert(surface != NULL && seat != NULL);

    struct pointer_state *pointer_state = double_buffer_surface_get_pointer_state(surface, seat);

    if (pointer_state)
        return pointer_state;

    pointer_state = calloc(1, sizeof(*pointer_state));
    pointer_state_init(pointer_state, seat);
    struct pointer_state *overwritten_pointer_state = NULL;
    hashset_insert(&surface->pointer_states, pointer_state, (void **)&overwritten_pointer_state);
    assert(overwritten_pointer_state == NULL);

    return pointer_state;
}

void double_buffer_surface_remove_pointer_state_by_seat(struct double_buffer_surface *surface, struct seat *seat)
{
    assert(surface != NULL && seat != NULL);

    struct pointer_state pointer_state_to_find = { .seat = seat };
    struct pointer_state *pointer_state = hashset_find(&surface->pointer_states, &pointer_state_to_find);
    
    if (!pointer_state)
        return;

    hashset_delete(&surface->pointer_states, pointer_state);
    pointer_state_destroy(pointer_state);
}

#pragma endregion
