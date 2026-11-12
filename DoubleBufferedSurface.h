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

#include "Buffer.h"

#include "Vector.h"
#include "SWRender/SWRender.h"

#include "Pointer.h"
#include "Keyboard.h"

struct DoubleBufferedSurface
{
    struct wl_surface  *wl_surface;
    struct wl_callback *wl_callback;

    int32_t w, h;

    struct Buffer buffer[2];
    struct Vector damage_rects[2];  // typeof(struct Buffer_Rect)
    
    unsigned int commit_count;

    SWRenderBuffer render_buffer;

    char *name;
    
    // keyboard
    struct Keyboard *keyboard;

    // pointer
    struct Pointer *pointer;
    uint32_t pointer_enter_serial;
    struct PointerState pointer_state;
};

extern void DoubleBufferedSurface_Init(struct DoubleBufferedSurface *surface);
extern void DoubleBufferedSurface_SetSize(struct DoubleBufferedSurface *surface, int32_t w, int32_t h);
extern void DoubleBufferedSurface_Cleanup(struct DoubleBufferedSurface *surface);

extern void DoubleBufferedSurface_SetName(struct DoubleBufferedSurface *surface, const char *name);

extern void DoubleBufferedSurface_AddDamage(struct DoubleBufferedSurface *surface, const struct Buffer_Rect *rect);
extern bool DoubleBufferedSurface_TryCommit(struct DoubleBufferedSurface *surface);
