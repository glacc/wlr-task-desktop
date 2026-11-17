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

#include <wayland-client.h>
///#include "viewporter-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "double_buffer_surface.h"

#include "SWRender/SWRender.h"
#include "SWRender/FTWordWrap.h"

#include "linked_list.h"
#include "hashset.h"
#include "vector.h"

struct toplevel_handle
{
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle;

    char *title;
    bool title_changed;

    char *app_id;
    bool app_id_changed;

    bool minimized;
    bool minimized_changed;

    SWRenderBuffer swrender_buffer_icon;
    bool icon_changed;

    struct hashset is_in_desktops;
    bool desktop_changed;
    struct desktop *iconify_to_desktop;

    bool closed;
};

struct desktop_icon_geometry
{
    int x;
    int y;
    int draw_w;
    int draw_h;
    int draw_origin_x;
    int draw_origin_y;

    bool valid;
    bool visible;
};

struct desktop_icon
{
    struct toplevel_handle *toplevel_handle;

    SWRenderBuffer swrender_buffer_icon_scaled;
    char *title;
    FTWordWrapLineBuffer title_line_buffer;
    SWRenderBuffer swrender_buffer_content;

    struct desktop_icon_geometry geometry;
    struct desktop_icon_geometry geometry_last_render;

    bool recalculate_position_when_minimize;

    bool is_selected;

    bool send_to_front;

    // only consumed after rendering
    bool redraw_content;
    bool redraw;
};

struct desktop
{
    // output related
    struct wl_output *wl_output;
    uint32_t wl_output_name;

    char *output_name;

    enum wl_output_transform output_transform;
    int32_t output_w_original;
    int32_t output_h_original;
    int32_t output_w;
    int32_t output_h;
    bool has_valid_output_mode;

    bool dpi_changed;

    // desktop icon related
    struct linked_list desktop_icons;   // struct desktop_icon *
    bool icon_loaded;

    struct pointer_state *is_operating_by_pointer_state;
    struct desktop_icon *selected_desktop_icon;
    int icon_drag_start_x;
    int icon_drag_start_y;

    int icon_w;
    int icon_h;
    float icon_corner_radius;
    int icon_spacing;

    float icon_title_font_size;

    // desktop surface related
    struct double_buffer_surface double_buffer_surface;
    struct zwlr_layer_surface_v1 *zwlr_layer_surface;
    //struct wp_viewport *wp_viewport;
    struct vector damaged_regions;
    SWRenderBoundI region_to_redraw;
    bool no_layer_surface_yet;
    bool damaged;

    bool removed;
};

#pragma region toplevel_handles

extern void toplevel_handles_init(void);
extern void toplevel_handles_cleanup(void);

extern void toplevel_handles_manage_icon_loading(void);
extern void toplevel_handles_manage_post_update(void);
extern void toplevel_handles_manage_closed(void);

#pragma endregion

#pragma region foreign_toplevel_manager

extern void foreign_toplevel_manager_init(struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager_v1);
extern void foreign_toplevel_manager_cleanup(void);

#pragma endregion

#pragma region desktops

extern void desktops_init(void);
extern void desktops_cleanup(void);

extern void desktops_add_by_wl_output(struct wl_output *wl_output, uint32_t name);
extern void desktops_pend_remove_by_name(uint32_t name);

extern void desktops_manage_creation(void);
extern void desktops_manage(void);
extern void desktops_manage_post_update(void);
extern void desktops_manage_removal(void);

#pragma endregion
