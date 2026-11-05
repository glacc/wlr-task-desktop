#include "DoubleBufferedSurface.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

static void DoubleBufferedSurface_CopyRectFromSurfaceToBuffer(struct Buffer *dst, struct DoubleBufferedSurface *src, struct Buffer_Rect *rect)
{
    int32_t x1 = rect->x;
    int32_t x2 = x1 + rect->w;
    int32_t y1 = rect->y;
    int32_t y2 = y1 + rect->h;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 <= 0) return;
    if (y2 <= 0) return;
    if (x2 > src->w) x2 = src->w;
    if (y2 > src->h) y2 = src->h;
    if (x2 > dst->w) x2 = dst->w;
    if (y2 > dst->h) y2 = dst->h;
    if (x1 >= x2) return;
    if (y1 >= y2) return;

    struct Buffer_Pixel *ptr_row_start_src = src->pixels + (y1 * src->w) + x1;
    struct Buffer_Pixel *ptr_row_start_dst = dst->pixels + (y1 * dst->w) + x1;
    size_t cpy_len = (x2 - x1) * sizeof(struct Buffer_Pixel);

    while (y1 < y2)
    {
        memcpy(ptr_row_start_dst, ptr_row_start_src, cpy_len);
        ptr_row_start_src += src->w;
        ptr_row_start_dst += dst->w;
        y1++;
    }
}

bool DoubleBufferedSurface_Init(struct DoubleBufferedSurface *surface, struct AppState *app_state)
{
    if (!surface || !app_state)
        return false;

    // surface creation
    surface->surface = wl_compositor_create_surface(app_state->compositor);
    if (!surface->surface)
    {
        fprintf(stderr, "failed to create surface: %s\n", strerror(errno));
        return false;
    }

    // make buffer internals NULL
    // damage_rects vector 0-1 initialization
    for (int i = 0; i < 2; i++)
    {
        surface->buffer[i].buffer = NULL;
        surface->buffer[i].pixels = NULL;

        if (!Vector_Init(&surface->damage_rects[i], sizeof(struct Buffer_Rect)))
        {
            DoubleBufferedSurface_Cleanup(surface);
            return false;
        }
    }

    return true;
}

bool DoubleBufferedSurface_SetSize(struct DoubleBufferedSurface *surface, struct AppState *app_state, int32_t w, int32_t h)
{
    if (!surface || !app_state)
        return false;

    if (!surface->surface)
        return false;

    if (surface->pixels)
    {
        free(surface->pixels);
        surface->pixels = NULL;
    }

    // buffer 0-1 reallocation
    for (int i = 0; i < 2; i++)
    {
        Buffer_Cleanup(&surface->buffer[i]);

        if (!Buffer_Init(&surface->buffer[i], app_state, w, h))
            goto Error;

        Vector_Clear(&surface->damage_rects[i]);
    }

    if (!(surface->pixels = malloc(sizeof(*surface->pixels) * w * h)))
        goto Error;

    surface->current_buffer = 0;
    surface->w = w;
    surface->h = h;

    return true;

Error:
    DoubleBufferedSurface_Cleanup(surface);

    return false;
}

void DoubleBufferedSurface_Cleanup(struct DoubleBufferedSurface *surface)
{
    if (!surface)
        return;

    if (surface->surface)
    {
        wl_surface_destroy(surface->surface);
        surface->surface = NULL;
    }

    for (int i = 0; i < 2; i++)
    {
        Buffer_Cleanup(&surface->buffer[i]);
        Vector_Cleanup(&surface->damage_rects[i]);
    }

    if (surface->pixels)
    {
        free(surface->pixels);
        surface->pixels = NULL;
    }
}

bool DoubleBufferedSurface_AddDamage(struct DoubleBufferedSurface *surface, const struct Buffer_Rect *rect)
{
    if (!surface)
        return false;

    struct Vector *vec_damage_rects = &surface->damage_rects[surface->current_buffer];

    if (rect)
        return Vector_Add(vec_damage_rects, (void *)rect);
    else
        return Vector_Add(vec_damage_rects, &(struct Buffer_Rect){ 0, 0, surface->w, surface->h });
}

bool DoubleBufferedSurface_TryCommit(struct DoubleBufferedSurface *surface)
{
    if (!surface)
        return false;

    if (!surface->surface)
        return false;

    struct Buffer *buffer_curr = &surface->buffer[surface->current_buffer];
    if (!buffer_curr->available)
        return false;

    unsigned int prev_or_flip_buffer = surface->current_buffer ^ 1;
    struct Buffer *buffer_prev = &surface->buffer[prev_or_flip_buffer];
    struct Buffer_Rect *rect;
    struct Vector_Iter iter = { };

    wl_surface_attach(surface->surface, buffer_curr->buffer, 0, 0);

    // damage_rects of buffer[prev_buffer]
    Vector_IterInit(&iter, &surface->damage_rects[prev_or_flip_buffer]);
    while ((rect = Vector_IterNext(&iter)))
    {
        DoubleBufferedSurface_CopyRectFromSurfaceToBuffer(buffer_prev, surface, rect);
        wl_surface_damage_buffer(surface->surface, rect->x, rect->y, rect->w, rect->h);
    }

    // damage_rects of buffer[curr_buffer]
    Vector_IterInit(&iter, &surface->damage_rects[surface->current_buffer]);
    while ((rect = Vector_IterNext(&iter)))
    {
        DoubleBufferedSurface_CopyRectFromSurfaceToBuffer(buffer_curr, surface, rect);
        wl_surface_damage_buffer(surface->surface, rect->x, rect->y, rect->w, rect->h);
    }

    buffer_curr->available = false;

    // flip
    Vector_Clear(&surface->damage_rects[prev_or_flip_buffer]);
    surface->current_buffer = prev_or_flip_buffer;

    wl_surface_commit(surface->surface);

    return true;
}
