#include "DoubleBufferedSurface.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

#include "Keyboard.h"
#include "Pointer.h"

#include "Utility.h"

#include "Globals.h"

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
    if (x2 > src->swr_buf.w) x2 = src->swr_buf.w;
    if (y2 > src->swr_buf.h) y2 = src->swr_buf.h;
    if (x2 > dst->w) x2 = dst->w;
    if (y2 > dst->h) y2 = dst->h;
    if (x1 >= x2) return;
    if (y1 >= y2) return;

    SWRenderColor       *ptr_row_start_src = SWRender_GetPointerByPosition(&src->swr_buf, x1, y1);
    struct Buffer_Pixel *ptr_row_start_dst = dst->pixels + (y1 * dst->w) + x1;

    while (y1 < y2)
    {
        SWRenderColor       *ptr_src = ptr_row_start_src;
        struct Buffer_Pixel *ptr_dst = ptr_row_start_dst;

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

        ptr_row_start_src = SWRender_GetPointerNextLine(&src->swr_buf, ptr_row_start_src);
        ptr_row_start_dst += dst->w;
        y1++;
    }
}

bool DoubleBufferedSurface_Init(struct DoubleBufferedSurface *surface)
{
    if (!surface || !wl_compositor)
        return false;

    surface->keyboard = NULL;
    surface->pointer  = NULL;

    surface->name = NULL;

    surface->commit_time[0] = surface->commit_time[1] = 10;
    surface->callback_followed_commit_count = 0;

    // surface creation
    surface->surface = wl_compositor_create_surface(wl_compositor);
    if (!surface->surface)
    {
        fprintf(stderr, "failed to create surface: %s\n", strerror(errno));
        return false;
    }

    wl_surface_set_user_data(surface->surface, surface);

    // reserved for future use?
    surface->callback = NULL;

    // pointer state
    if (!PointerState_Init(&surface->pointer_state))
    {
        fprintf(stderr, "failed to init pointer state for surface: %s\n", strerror(errno));
        DoubleBufferedSurface_Cleanup(surface);
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
            fprintf(stderr, "failed to init damage rect vector for surface: %s\n", strerror(errno));
            DoubleBufferedSurface_Cleanup(surface);
            return false;
        }
    }

    return true;
}

bool DoubleBufferedSurface_SetSize(struct DoubleBufferedSurface *surface, int32_t w, int32_t h)
{
    if (!surface)
        return false;

    if (!surface->surface)
        return false;

    SWRender_BufferFree(&surface->swr_buf);

    // buffer 0-1 reallocation
    for (int i = 0; i < 2; i++)
    {
        Buffer_Cleanup(&surface->buffer[i]);

        if (!Buffer_Init(&surface->buffer[i], w, h))
            goto Error;

        Vector_Clear(&surface->damage_rects[i]);
    }

    surface->swr_buf.w = w;
    surface->swr_buf.h = h;
    if (!SWRender_BufferAlloc(&surface->swr_buf))
        goto Error;

    surface->commit_count = 0;

    return true;

Error:
    DoubleBufferedSurface_Cleanup(surface);

    return false;
}

void DoubleBufferedSurface_Cleanup(struct DoubleBufferedSurface *surface)
{
    if (!surface)
        return;

    if (surface->name)
        free(surface->name);

    if (surface->callback)
    {
        wl_callback_destroy(surface->callback);
        surface->callback = NULL;
    }

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

    // unref this
    if (surface->pointer)
    {
        HashSet_Del(&surface->pointer->surfaces_to_manage, surface);
        if (surface->pointer->surface == surface)
            surface->pointer->surface = NULL;
    }

    PointerState_Cleanup(&surface->pointer_state);

    SWRender_BufferFree(&surface->swr_buf);
}

bool DoubleBufferedSurface_AddDamage(struct DoubleBufferedSurface *surface, const struct Buffer_Rect *rect)
{
    if (!surface)
        return false;

    struct Vector *vec_damage_rects = &surface->damage_rects[surface->commit_count & 1];

    if (rect)
        return Vector_Add(vec_damage_rects, (void *)rect);
    else
        return Vector_Add(vec_damage_rects, &(struct Buffer_Rect){ 0, 0, surface->swr_buf.w, surface->swr_buf.h });
}

bool DoubleBufferedSurface_TryCommit(struct DoubleBufferedSurface *surface)
{
    if (!surface)
        return false;

    if (!surface->surface)
        return false;

    unsigned int current_buffer = surface->commit_count & 1;
    struct Buffer *buffer_curr = &surface->buffer[current_buffer];

    if (!buffer_curr->available)
        return false;

    // attach current buffer
    wl_surface_attach(surface->surface, buffer_curr->buffer, 0, 0);

    unsigned int prev_or_flip_buffer = current_buffer ^ 1;
    struct Buffer *buffer_prev = &surface->buffer[prev_or_flip_buffer];
    
    struct Buffer_Rect *rect;
    struct Vector_Iter iter = { };

    printf("use plane %d\n", current_buffer);

    for (int i = 0; i < (sizeof(surface->damage_rects) / sizeof(surface->damage_rects[0])); i++)
    {
        Vector_IterInit(&iter, &surface->damage_rects[i]);
        while ((rect = Vector_IterNext(&iter)))
        {
            DoubleBufferedSurface_CopyRectFromSurfaceToBuffer(buffer_curr, surface, rect);
            wl_surface_damage_buffer(surface->surface, rect->x, rect->y, rect->w, rect->h);
            
            printf("damage at (%d, %d) in size (%d, %d)\n", rect->x, rect->y, rect->w, rect->h);
        }
    }
    Vector_Clear(&surface->damage_rects[prev_or_flip_buffer]);

    // flip
    wl_surface_commit(surface->surface);
    buffer_curr->available = false;

    surface->commit_count++;
    surface->commit_time[current_buffer] = surface->commit_count;

    return true;
}

bool DoubleBufferedSurface_SetName(struct DoubleBufferedSurface *surface, const char *name)
{
    if (!surface)
        return false;

    Utility_TryReplaceWithAnotherString(&surface->name, name);

    return (surface->name != NULL);
}
