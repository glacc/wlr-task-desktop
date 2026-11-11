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

#include <wayland-client.h>

#include "HashSet.h"

struct KeyboardState
{

};

struct Keyboard
{
    struct wl_keyboard *wl_keyboard;

    struct DoubleBufferedSurface *surface;

    // typeof(struct DoubleBufferedSurface *)
    struct HashSet surfaces_to_manage;
};

extern void Keyboard_GetWlObj(struct Keyboard *keyboard, struct wl_seat *seat);
extern void Keyboard_DestroyWlObj(struct Keyboard *keyboard);

extern const struct wl_keyboard_listener Keyboard_listener;
