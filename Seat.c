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

#include "Seat.h"

#include <stdio.h>

#include "Keyboard.h"
#include "Pointer.h"

#include "Globals.h"

static void Seat_Capabilities(
    void *data,
    struct wl_seat *wl_seat,
    uint32_t capabilities
)
{
    struct AppState *app_state = data;

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
        Keyboard_GetWlObj(&global_keyboard, wl_seat);
    else
        Keyboard_DestroyWlObj(&global_keyboard);
    
    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
        Pointer_GetWlObj(&global_pointer, wl_seat);
    else
        Pointer_DestroyWlObj(&global_pointer);
}

static void Seat_Name(
    void *data,
    struct wl_seat *wl_seat,
    const char *name
)
{
    printf("seat name: %s\n", name);
}

const struct wl_seat_listener Seat_listener =
{
    .capabilities = Seat_Capabilities,
    .name = Seat_Name,
};
