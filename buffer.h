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

#include <wayland-client.h>

struct buffer_rect
{
    int32_t x, y;
    int32_t w, h;
};

struct buffer_pixel
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
}
__attribute__((packed));

struct buffer
{
    struct wl_buffer *wl_buffer;
    struct buffer_pixel *pixels;
    
    int32_t w, h;

    bool available;

    bool valid;
};

static inline struct buffer_pixel *buffer_ptr_offset_by_bytes(struct buffer_pixel *ptr, size_t offset)
{
    return (struct buffer_pixel *)((uint8_t *)ptr + offset);
}

extern void buffer_init(struct buffer *, int32_t w, int32_t h);
extern void buffer_cleanup(struct buffer *buffer);
