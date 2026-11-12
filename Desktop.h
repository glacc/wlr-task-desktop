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

#include <stdbool.h>

#include <wayland-client.h>

#include "LinkedList.h"
#include "DesktopSurface.h"

#include "Vector.h"

struct Desktop
{
    // output
    struct wl_output *wl_output;

    uint32_t wl_name;

    char *output_name;
    char *output_description;

    int32_t output_x, output_y;
    int32_t output_phy_w, output_phy_h;

    int32_t w, h;

    int32_t scale;

    // surface
    struct DesktopSurface desktop_surface;
    bool surface_created;

    struct Vector damage_bounds;   // typeof(SWRenderBoundI)
    bool damaged;

    // toplevel handle
    struct LinkedList toplevel_handles; // typeof(struct ToplevelHandle *)
    struct LinkedList_Node *toplevel_handle_to_send_front;

    // selected handle and state
    struct ToplevelHandle *selected_handle;
    int icon_drag_start_x;
    int icon_drag_start_y;

    bool debug_key_pressed;
};

extern const struct wl_output_listener Desktop_output_wl_listener;

extern size_t Desktop_Hash(void *ptr);
extern bool Desktop_IsEqual(void *l, void *r);

extern struct Desktop *Desktop_Create(void);
extern void Desktop_Destroy(struct Desktop *desktop);

extern void Desktop_AddDamage(struct Desktop *desktop, int32_t x, int32_t y, int32_t w, int32_t h);
extern void Desktop_AddDamageToplevelHandle(struct Desktop *desktop, struct ToplevelHandle *toplevel_handle);

extern void Desktop_Update(struct Desktop *desktop);
extern void Desktop_Draw(struct Desktop *desktop);
extern bool Desktop_TryCommit(struct Desktop *desktop);
