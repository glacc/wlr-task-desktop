#pragma once

#include "Buffer.h"

#include "Vector.h"
#include "SWRender/SWRender.h"

#include "Pointer.h"
#include "Keyboard.h"

struct DoubleBufferedSurface
{
    struct wl_surface  *surface;
    struct wl_callback *callback;

    struct Buffer buffer[2];
    struct Vector damage_rects[2];  // typeof(struct Buffer_Rect)
    
    unsigned int commit_count;
    unsigned int commit_time[2];
    unsigned int callback_followed_commit_count;

    SWRenderBuffer swr_buf;

    char *name;
    
    // keyboard
    struct Keyboard *keyboard;

    // pointer
    struct Pointer *pointer;
    struct PointerState pointer_state;
};

extern bool DoubleBufferedSurface_Init(struct DoubleBufferedSurface *surface);
extern bool DoubleBufferedSurface_SetSize(struct DoubleBufferedSurface *surface, int32_t w, int32_t h);
extern void DoubleBufferedSurface_Cleanup(struct DoubleBufferedSurface *surface);

extern bool DoubleBufferedSurface_AddDamage(struct DoubleBufferedSurface *surface, const struct Buffer_Rect *rect);
extern bool DoubleBufferedSurface_TryCommit(struct DoubleBufferedSurface *surface);

extern bool DoubleBufferedSurface_SetName(struct DoubleBufferedSurface *surface, const char *name);
