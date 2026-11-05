#pragma once

#include <stdbool.h>

#include "AppState.h"

struct Buffer_Rect
{
    int32_t x, y;
    int32_t w, h;
};

struct Buffer_Pixel
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
}
__attribute__((packed));

struct Buffer
{
    struct wl_buffer *buffer;
    struct Buffer_Pixel *pixels;
    
    int32_t w, h;

    bool available;
};

static inline struct Buffer_Pixel *Buffer_PtrOffsetByByte(struct Buffer_Pixel *ptr, size_t offset)
{
    return (struct Buffer_Pixel *)((uint8_t *)ptr + offset);
}

extern bool Buffer_Init(struct Buffer *buffer, struct AppState *app_state, int32_t w, int32_t h);
extern void Buffer_Cleanup(struct Buffer *buffer);
