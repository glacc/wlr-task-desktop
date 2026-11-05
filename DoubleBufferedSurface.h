#pragma once

#include "Buffer.h"
#include "AppState.h"

#include "Vector.h"

struct DoubleBufferedSurface
{
    struct wl_surface *surface;

    struct Buffer buffer[2];
    struct Vector damage_rects[2];  // typeof(struct Buffer_Rect)
    unsigned int current_buffer;

    int32_t w, h;

    struct Buffer_Pixel *pixels;
};

extern bool DoubleBufferedSurface_Init(struct DoubleBufferedSurface *surface, struct AppState *app_state);
extern bool DoubleBufferedSurface_SetSize(struct DoubleBufferedSurface *surface, struct AppState *app_state, int32_t w, int32_t h);
extern void DoubleBufferedSurface_Cleanup(struct DoubleBufferedSurface *surface);

extern bool DoubleBufferedSurface_AddDamage(struct DoubleBufferedSurface *surface, const struct Buffer_Rect *rect);
extern bool DoubleBufferedSurface_TryCommit(struct DoubleBufferedSurface *surface);
