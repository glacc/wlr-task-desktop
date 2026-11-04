#pragma once

#include "Buffer.h"

#include "Vector.h"

struct DoubleBufferedSurface
{
    struct wl_surface *surface;

    struct Buffer buffer[2];
    struct Vector damage_rects[2];
    int current_buffer;

    struct Buffer_Pixel *pixels;
};

extern bool DoubleBufferedSurface_Init(struct DoubleBufferedSurface *surface);
extern void DoubleBufferedSurface_Cleanup(struct DoubleBufferedSurface *surface);
