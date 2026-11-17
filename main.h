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

#include <xkbcommon/xkbcommon.h>

#include <wayland-client.h>
#include "cursor-shape-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "SWRender/FTEssentials.h"

extern struct wl_compositor *wl_compositor;
extern struct wl_shm        *wl_shm;

extern struct wp_cursor_shape_manager_v1 *wp_cursor_shape_manager;

extern struct zwlr_layer_shell_v1 *zwlr_layer_shell;

extern struct xkb_context *xkb_context;

extern FTEssentials_State ft_state;
