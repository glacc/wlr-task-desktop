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
#include <stddef.h>
#include <stdbool.h>

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

extern bool Buffer_Init(struct Buffer *, int32_t w, int32_t h);
extern void Buffer_Cleanup(struct Buffer *buffer);
