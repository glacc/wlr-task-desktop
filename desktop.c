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

// TODO: split this file?

#include "desktop.h"

#include <assert.h>

#include <linux/input-event-codes.h>

#include "main.h"
#include "seat.h"
#include "icon.h"

#include "debug.h"

#include "SWRender/SWRender.h"
#include "SWRender/SWRenderExtImage.h"
#include "SWRender/FTRenderWordWrap.h"

#define DESKTOP_ICON_DEFAULT_W                  96
#define DESKTOP_ICON_DEFAULT_H                  112
#define DESKTOP_ICON_DEFAULT_CORNER_RADIUS      5.0F
#define DESKTOP_ICON_DEFAULT_SPACING            2

#define DESKTOP_ICON_DEFAULT_ICON_TOP_PADDING   4
#define DESKTOP_ICON_DEFAULT_ICON_SIZE_X        48
#define DESKTOP_ICON_DEFAULT_ICON_SIZE_Y        48
#define DESKTOP_ICON_DEFAULT_ICON_TEXT_SPACING  4
#define DESKTOP_ICON_DEFAULT_TEXT_SIZE          8.5F
#define DESKTOP_ICON_DEFAULT_TEXT_LINESPACE_MUL 0.775F

static struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager = NULL;

static struct hashset toplevel_handles = { };   // struct toplevel_handle *
static struct hashset desktops = { };           // struct desktop *
static struct linked_list toplevel_handles_to_remove = { }; // struct toplevel_handle *
static struct linked_list desktops_to_remove = { };         // struct desktop *

static bool is_foreign_toplevel_manager_finished = false;

struct desktop_icon_gradient
{
    SWRenderColor color;
    float position;
};

static const struct desktop_icon_gradient desktop_icon_gradients[] =
{
    { { 0xFF, 0xFF, 0xFF, 0x30 }, 0.00F },
    { { 0xFF, 0xFF, 0xFF, 0x30 }, 0.00F },
    { { 0xFF, 0xFF, 0xFF, 0x53 }, 0.25F },
    { { 0xFF, 0xFF, 0xFF, 0x39 }, 0.60F },
    { { 0xFF, 0xFF, 0xFF, 0x15 }, 1.00F },
    { { 0xFF, 0xFF, 0xFF, 0x15 }, 1.00F },
};
static const int desktop_icon_gradients_count = (sizeof(desktop_icon_gradients) / sizeof(desktop_icon_gradients[0]));

#pragma region desktop_icon

static struct desktop_icon *desktop_icon_create(struct toplevel_handle *toplevel_handle, struct desktop *desktop)
{
    struct desktop_icon *desktop_icon = calloc(1, sizeof(*desktop_icon));
    assert(desktop_icon != NULL);

    desktop_icon->toplevel_handle = toplevel_handle;

    desktop_icon->recalculate_position_when_minimize = true;

    FTWordWrapLineBuffer_Init(&desktop_icon->title_line_buffer);

    desktop_icon->redraw_content = true;

    return desktop_icon;
}

static void desktop_icon_destroy(struct desktop_icon *desktop_icon)
{
    assert(desktop_icon != NULL);

    SWRender_BufferFree(&desktop_icon->swrender_buffer_icon_scaled);
    FTWordWrapLineBuffer_Cleanup(&desktop_icon->title_line_buffer);
    SWRender_BufferFree(&desktop_icon->swrender_buffer_content);

    if (desktop_icon->title)
        free(desktop_icon->title);

    free(desktop_icon);
}

static inline bool desktop_icon_geometry_is_visible(const struct desktop_icon_geometry *desktop_icon_geometry)
{
    return (desktop_icon_geometry->visible && desktop_icon_geometry->valid);
}

static inline bool desktop_icon_geometry_is_position_equal(const struct desktop_icon_geometry *l, const struct desktop_icon_geometry *r)
{
    bool position_equal = (l->x == r->x) && (l->y == r->y);
    bool origin_equal   = (l->draw_origin_x == r->draw_origin_x) && (l->draw_origin_y == r->draw_origin_y);
    return (position_equal && origin_equal);
}

static inline bool desktop_icon_geometry_is_size_equal(const struct desktop_icon_geometry *l, const struct desktop_icon_geometry *r)
{
    return (l->draw_w == r->draw_w) && (l->draw_h == r->draw_h);
}

static bool desktop_icon_geometry_is_bounds_equal(const struct desktop_icon_geometry *l, const struct desktop_icon_geometry *r)
{
    bool position_equal = desktop_icon_geometry_is_position_equal(l, r);
    bool size_equal     = desktop_icon_geometry_is_size_equal(l, r);
    bool origin_equal   = (l->draw_origin_x == r->draw_origin_x) && (l->draw_origin_y == r->draw_origin_y);
    return (position_equal && size_equal && origin_equal);
}

static bool desktop_icon_geometry_is_position_inside(const struct desktop_icon_geometry *desktop_icon_geometry, const struct desktop *desktop, int x, int y)
{
    bool horz_inside = (x >= desktop_icon_geometry->x) && (x < desktop_icon_geometry->x + desktop->icon_w);
    bool vert_inside = (y >= desktop_icon_geometry->y) && (y < desktop_icon_geometry->y + desktop->icon_h);
    return horz_inside && vert_inside;
}

static void desktop_icon_geometry_damage_desktop(const struct desktop_icon_geometry *desktop_icon_geometry, struct desktop *desktop)
{
    if (!desktop_icon_geometry->valid)
        return;

    int x1 = desktop_icon_geometry->x + desktop_icon_geometry->draw_origin_x;
    int y1 = desktop_icon_geometry->y + desktop_icon_geometry->draw_origin_y;
    int x2 = x1 + desktop_icon_geometry->draw_w - 1;
    int y2 = y1 + desktop_icon_geometry->draw_h - 1;

    vector_insert(&desktop->damaged_regions, &(SWRenderBoundI){ x1, y1, x2, y2 });
}

static bool desktop_icon_has_intersection_between(struct desktop *desktop, struct desktop_icon *l, struct desktop_icon *r)
{
    SWRenderBoundI bound_l = 
    {
        l->geometry.x,
        l->geometry.y,
        l->geometry.x + desktop->icon_w - 1,
        l->geometry.y + desktop->icon_h - 1,
    };
    SWRenderBoundI bound_r =
    {
        r->geometry.x,
        r->geometry.y,
        r->geometry.x + desktop->icon_w - 1,
        r->geometry.y + desktop->icon_h - 1,
    };
    return SWRender_GetIntersectionBetweenBoundInt(&bound_l, &bound_r, NULL);
}

static void desktop_icon_render_gradients(struct desktop_icon *desktop_icon)
{
    int y = 0;

    SWRenderColor *ptr_row_start = SWRender_GetPointerByPosition(&desktop_icon->swrender_buffer_content, 0, 0);

    for (int i = 1; i < desktop_icon_gradients_count - 1; i++)
    {
        int y_start_this_gradient = desktop_icon->swrender_buffer_content.h * desktop_icon_gradients[i - 1].position;
        int y_end_this_gradient   = desktop_icon->swrender_buffer_content.h * desktop_icon_gradients[i].position;
        float progress_div_mul = 1.0F / (float)(y_end_this_gradient - y_start_this_gradient);

        if (y_end_this_gradient > desktop_icon->swrender_buffer_content.h)
            y_end_this_gradient = desktop_icon->swrender_buffer_content.h;
        
        while (y < y_end_this_gradient)
        {
            float progress = (y - y_start_this_gradient) * progress_div_mul;
            SWRenderColor color =
            {
                .r = (uint8_t)((float)desktop_icon_gradients[i - 1].color.r + ((float)((int)desktop_icon_gradients[i].color.r - (int)desktop_icon_gradients[i - 1].color.r) * progress)),
                .g = (uint8_t)((float)desktop_icon_gradients[i - 1].color.g + ((float)((int)desktop_icon_gradients[i].color.g - (int)desktop_icon_gradients[i - 1].color.g) * progress)),
                .b = (uint8_t)((float)desktop_icon_gradients[i - 1].color.b + ((float)((int)desktop_icon_gradients[i].color.b - (int)desktop_icon_gradients[i - 1].color.b) * progress)),
                .a = (uint8_t)((float)desktop_icon_gradients[i - 1].color.a + ((float)((int)desktop_icon_gradients[i].color.a - (int)desktop_icon_gradients[i - 1].color.a) * progress)),
            };
            SWRenderColor *ptr_pixel = ptr_row_start;

            for (int x = 0; x < desktop_icon->swrender_buffer_content.w; x++)
                *ptr_pixel++ = color;

            ptr_row_start = SWRender_GetPointerNextLine(&desktop_icon->swrender_buffer_content, ptr_row_start);

            y++;
        }
    }
}

static void desktop_icon_manage_may_pend_redraw_content(struct desktop_icon *desktop_icon, struct desktop *desktop)
{
    if (desktop_icon->toplevel_handle->closed)
        return;
    
    bool selected   = !desktop_icon->is_selected && (desktop->selected_desktop_icon == desktop_icon);
    bool unselected = desktop_icon->is_selected  && (desktop->selected_desktop_icon != desktop_icon);
    
    if (selected)
    {
        desktop_icon->is_selected = true;
        desktop_icon->redraw_content = true;
    }
    if (unselected)
    {
        desktop_icon->is_selected = false;
        desktop_icon->redraw_content = true;
    }

    if (desktop_icon->toplevel_handle->title_changed || desktop_icon->toplevel_handle->app_id_changed || desktop_icon->toplevel_handle->icon_changed)
        desktop_icon->redraw_content = true;
}

static void desktop_icon_manage_visibility(struct desktop_icon *desktop_icon, struct desktop *desktop)
{
    if (desktop_icon->toplevel_handle->closed)
    {
        desktop_icon->geometry.visible = false;
        return;
    }

    bool is_toplevel_handle_in_desktop = (desktop_icon->toplevel_handle->iconify_to_desktop == desktop);
    bool is_minimized = desktop_icon->toplevel_handle->minimized;

    desktop_icon->geometry.visible = is_toplevel_handle_in_desktop && is_minimized;
}

static void desktop_icon_manage_may_pend_send_to_front(struct desktop_icon *desktop_icon, struct desktop *desktop)
{
    if (desktop_icon->toplevel_handle->closed)
    {
        desktop_icon->send_to_front = false;
        return;
    }

    bool is_toplevel_handle_in_desktop = (desktop_icon->toplevel_handle->iconify_to_desktop == desktop);
    bool is_minimizing = (desktop_icon->toplevel_handle->minimized_changed && desktop_icon->toplevel_handle->minimized);

    if (is_toplevel_handle_in_desktop && is_minimizing && desktop_icon->recalculate_position_when_minimize)
        desktop_icon->send_to_front = true;
}

static void desktop_icon_manage_position_may_recalculation(struct desktop_icon *desktop_icon, struct desktop *desktop)
{
    if (desktop_icon->toplevel_handle->closed)
        return;
    
    // recalculation position when minimized or visible without valid position
    // !recalculate_position_when_minimize also means user had dragged the icon so memorize it
    bool recalculate_position = false;
    
    if (desktop->dpi_changed && desktop_icon->recalculate_position_when_minimize)
        recalculate_position = true;
    else if (desktop_icon->recalculate_position_when_minimize && desktop_icon->toplevel_handle->minimized_changed && desktop_icon->toplevel_handle->minimized)
        recalculate_position = true;
    else if (desktop_icon->geometry.visible && !desktop_icon->geometry.valid)
        recalculate_position = true;

    if (!recalculate_position)
        return;

    int desktop_x_step = desktop->icon_w + desktop->icon_spacing;
    int desktop_y_step = desktop->icon_h + desktop->icon_spacing;

    desktop_icon->geometry.x = desktop->icon_spacing;
    desktop_icon->geometry.y = desktop->double_buffer_surface.h - desktop_y_step - 1;

    while (true)
    {
        // overlap check
        bool has_overlap = false;

        struct linked_list_iterator iterator_desktop_icons_overlap_check = { };
        struct linked_list_node *desktop_icon_overlap_check_node = NULL;
        struct desktop_icon *desktop_icon_overlap_check = NULL;
        linked_list_iterator_init(&iterator_desktop_icons_overlap_check, &desktop->desktop_icons);
        while ((desktop_icon_overlap_check_node = linked_list_iterator_next(&iterator_desktop_icons_overlap_check)))
        {
            desktop_icon_overlap_check = desktop_icon_overlap_check_node->elem;

            // skip self / closed / invisible
            if (desktop_icon == desktop_icon_overlap_check)
                continue;
            else if (desktop_icon_overlap_check->toplevel_handle->closed)
                continue;
            else if (!desktop_icon_geometry_is_visible(&desktop_icon_overlap_check->geometry))
                continue;

            if (desktop_icon_has_intersection_between(desktop, desktop_icon, desktop_icon_overlap_check))
            {
                has_overlap = true;
                break;
            }
        }

        if (!has_overlap)
            break;

        // step if overlap with others
        desktop_icon->geometry.x += desktop_x_step;
        if (desktop_icon->geometry.x >= desktop->double_buffer_surface.w - desktop_x_step)
        {
            desktop_icon->geometry.x = desktop->icon_spacing;
            desktop_icon->geometry.y -= desktop_y_step;
        }
    }

    desktop_icon->geometry.valid = true;
    desktop_icon->redraw = true;
}

static void desktop_icon_may_render_content(struct desktop_icon *desktop_icon, struct desktop *desktop)
{
    if (desktop_icon->toplevel_handle->closed)
        return;

    if (!desktop_icon_geometry_is_visible(&desktop_icon->geometry))
        return;

    if (!desktop_icon->redraw_content)
        return;

    // TODO: precalculate title lines here and resize geometry?

    // TODO: set size by title...?
    desktop_icon->geometry.draw_w = desktop->icon_w;
    desktop_icon->geometry.draw_h = desktop->icon_h;

    // reallocate 
    if (!SWRender_CheckBuffer(&desktop_icon->swrender_buffer_content))
    {
        desktop_icon->swrender_buffer_content.w = desktop_icon->geometry.draw_w;
        desktop_icon->swrender_buffer_content.h = desktop_icon->geometry.draw_h;
        SWRender_BufferAlloc(&desktop_icon->swrender_buffer_content);
    }
    else if (!desktop_icon_geometry_is_size_equal(&desktop_icon->geometry_last_render, &desktop_icon->geometry))
    {
        SWRender_BufferFree(&desktop_icon->swrender_buffer_content);
        desktop_icon->swrender_buffer_content.w = desktop_icon->geometry.draw_w;
        desktop_icon->swrender_buffer_content.h = desktop_icon->geometry.draw_h;
        bool allocated = SWRender_BufferAlloc(&desktop_icon->swrender_buffer_content);
        assert(allocated == true);
    }
    
    // background
    //SWRender_FillRect(&desktop_icon->swrender_buffer_content, NULL, (SWRenderColor){ 255, 255, 255, 63 }, true);
    desktop_icon_render_gradients(desktop_icon);

    // icon
    if (desktop_icon->toplevel_handle->icon_changed && SWRender_CheckBuffer(&desktop_icon->toplevel_handle->swrender_buffer_icon))
    {
        if (SWRender_CheckBuffer(&desktop_icon->swrender_buffer_icon_scaled))
            SWRender_BufferFree(&desktop_icon->swrender_buffer_icon_scaled);

        desktop_icon->swrender_buffer_icon_scaled.w = DESKTOP_ICON_DEFAULT_ICON_SIZE_X;
        desktop_icon->swrender_buffer_icon_scaled.h = DESKTOP_ICON_DEFAULT_ICON_SIZE_Y;
        SWRender_DupSWScaledToBuffer(&desktop_icon->toplevel_handle->swrender_buffer_icon, &desktop_icon->swrender_buffer_icon_scaled);

        desktop_icon->toplevel_handle->icon_changed = false;
    }

    if (SWRender_CheckBuffer(&desktop_icon->swrender_buffer_icon_scaled))
    {
        SWRender_CopyBuffer(&desktop_icon->swrender_buffer_icon_scaled, NULL, &desktop_icon->swrender_buffer_content, (SWRenderVec2I){
            (desktop->icon_w - DESKTOP_ICON_DEFAULT_ICON_SIZE_X) / 2,
            DESKTOP_ICON_DEFAULT_ICON_TOP_PADDING
        });
    }

    // title
    if (desktop_icon->title)
    {
        free(desktop_icon->title);
        desktop_icon->title = NULL;
    }

    if (desktop_icon->toplevel_handle->title)
    {
        desktop_icon->title = strdup(desktop_icon->toplevel_handle->title);
        assert(desktop_icon->title != NULL);

        int title_start_y = DESKTOP_ICON_DEFAULT_ICON_TOP_PADDING + DESKTOP_ICON_DEFAULT_ICON_SIZE_Y + DESKTOP_ICON_DEFAULT_ICON_TEXT_SPACING;
        SWRenderRectF title_render_rect =
        {
            0,
            title_start_y,
            desktop_icon->geometry.draw_w,
            desktop_icon->geometry.draw_h - title_start_y,
        };
        
        FTEssentials_SetSize96DPI(&ft_state, FLOAT_TO_F26DOT6(desktop->icon_title_font_size));
        ft_state.linespace_multiplier = DESKTOP_ICON_DEFAULT_TEXT_LINESPACE_MUL;
        FTRenderWordWrap_RenderStrWithAlign(
            &ft_state,
            &desktop_icon->title_line_buffer,
            &desktop_icon->swrender_buffer_content,
            &title_render_rect,
            desktop_icon->title,
            0, -1,
            (SWRenderColor){ .a = 255 }
        );

        // TODO: scale icon here?

    }

    if (desktop_icon->is_selected)
    {
        SWRenderColor *ptr_content_pixel_row_start = SWRender_GetPointerByPosition(&desktop_icon->swrender_buffer_content, 0, 0);

        for (int y = 0; y < desktop_icon->swrender_buffer_content.h; y++)
        {
            SWRenderColor *ptr_content_pixel = ptr_content_pixel_row_start;
            
            for (int x = 0; x < desktop_icon->swrender_buffer_content.w; x++)
            {
                ptr_content_pixel->r *= 0.5F;
                ptr_content_pixel->g *= 0.7F;
                ptr_content_pixel->b *= 1.0F;
                ptr_content_pixel->a *= 1.0F;
                ptr_content_pixel++;
            }

            ptr_content_pixel_row_start = SWRender_GetPointerNextLine(&desktop_icon->swrender_buffer_content, ptr_content_pixel_row_start);
        }
    }

    // consumed here for lazy visible content redraw
    desktop_icon->redraw_content = false;

    desktop_icon->redraw = true;
}

#pragma endregion

#pragma region toplevel_handle

#pragma region toplevel_handle_listeners

static void toplevel_handle_title(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1,
    const char *title
)
{
    struct toplevel_handle *toplevel_handle = data;
    char *title_old = NULL;

    if (toplevel_handle->title)
        title_old = toplevel_handle->title;

    toplevel_handle->title = title ? strdup(title) : NULL;

    toplevel_handle->title_changed = true;

    if (title_old)
    {
        debug_printf("title: %s\n\t-> %s\n", debug_str_nullable(title_old), debug_str_nullable(title));
        free(title_old);
    }
    else
        debug_printf("title: %s\n", debug_str_nullable(title));
}

static void toplevel_handle_app_id(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1,
    const char *app_id
)
{
    struct toplevel_handle *toplevel_handle = data;
    char *app_id_old = NULL;

    if (toplevel_handle->app_id)
        app_id_old = toplevel_handle->app_id;

    toplevel_handle->app_id = app_id ? strdup(app_id) : NULL;

    toplevel_handle->app_id_changed = true;

    if (app_id_old)
    {
        debug_printf("app_id: %s\n\t-> %s\n", debug_str_nullable(app_id_old), debug_str_nullable(app_id));
        free(app_id_old);
    }
    else
        debug_printf("app_id: %s\n", debug_str_nullable(app_id));
}

static void toplevel_handle_output_enter(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1,
    struct wl_output *wl_output
)
{
    struct toplevel_handle *toplevel_handle = data;
    
    if (!wl_output)
        return;

    struct desktop *desktop = wl_output_get_user_data(wl_output);
    hashset_insert(&toplevel_handle->is_in_desktops, desktop, NULL);
    toplevel_handle->desktop_changed = true;
}

static void toplevel_handle_output_leave(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1,
    struct wl_output *wl_output
)
{
    struct toplevel_handle *toplevel_handle = data;

    if (!wl_output)
        return;

    struct desktop *desktop = wl_output_get_user_data(wl_output);
    hashset_delete(&toplevel_handle->is_in_desktops, desktop);
    toplevel_handle->desktop_changed = true;
}

static void toplevel_handle_state(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1,
    struct wl_array *states
)
{
    struct toplevel_handle *toplevel_handle = data;

    bool minimized_old = toplevel_handle->minimized;

    toplevel_handle->minimized = false;

    uint32_t *state;
    wl_array_for_each(state, states)
    {
        if (*state == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
        {
            toplevel_handle->minimized = true;
            break;
        }
    }

    if (minimized_old != toplevel_handle->minimized)
        toplevel_handle->minimized_changed = true;

    debug_printf(
        "'%s' (%s) minimize state changed: %d -> %d\n",
        debug_str_nullable(toplevel_handle->title),
        debug_str_nullable(toplevel_handle->app_id),
        minimized_old, toplevel_handle->minimized
    );
}

static void toplevel_handle_done(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1
)
{
    struct toplevel_handle *toplevel_handle = data;

    if (toplevel_handle->desktop_changed)
    {
        if (toplevel_handle->is_in_desktops.count == 1)
        {
            struct hashset_iterator iterator_desktops = { };
            hashset_iterator_init(&iterator_desktops, &toplevel_handle->is_in_desktops);
            toplevel_handle->iconify_to_desktop = hashset_iterator_next(&iterator_desktops);
        }
        else if (toplevel_handle->is_in_desktops.count == 0)
            toplevel_handle->iconify_to_desktop = NULL;

        toplevel_handle->desktop_changed = false;
    }
}

static void toplevel_handle_closed(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1
)
{
    struct toplevel_handle *toplevel_handle = data;

    toplevel_handle->closed = true;

    debug_printf("'%s' (%s) closed\n", debug_str_nullable(toplevel_handle->title), debug_str_nullable(toplevel_handle->app_id));
}

static void toplevel_handle_parent(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle_v1,
    struct zwlr_foreign_toplevel_handle_v1 *parent
)
{
    // currently left blank (unused)
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener =
{
    .title        = toplevel_handle_title,
    .app_id       = toplevel_handle_app_id,
    .output_enter = toplevel_handle_output_enter,
    .output_leave = toplevel_handle_output_leave,
    .state        = toplevel_handle_state,
    .done         = toplevel_handle_done,
    .closed       = toplevel_handle_closed,
    .parent       = toplevel_handle_parent,
};

#pragma endregion

static struct toplevel_handle *toplevel_handle_create(struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle)
{
    assert(zwlr_foreign_toplevel_handle != NULL);

    struct toplevel_handle *toplevel_handle = calloc(1, sizeof(*toplevel_handle));
    assert(toplevel_handle != NULL);

    toplevel_handle->zwlr_foreign_toplevel_handle = zwlr_foreign_toplevel_handle;
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel_handle->zwlr_foreign_toplevel_handle, &toplevel_handle_listener, toplevel_handle);

    hashset_init(&toplevel_handle->is_in_desktops);

    return toplevel_handle;
}

static void toplevel_handle_destroy(struct toplevel_handle *toplevel_handle)
{
    assert(toplevel_handle != NULL && toplevel_handle->zwlr_foreign_toplevel_handle != NULL);

    zwlr_foreign_toplevel_handle_v1_destroy(toplevel_handle->zwlr_foreign_toplevel_handle);

    if (toplevel_handle->title)
        free(toplevel_handle->title);
    if (toplevel_handle->app_id)
        free(toplevel_handle->app_id);

    SWRender_BufferFree(&toplevel_handle->swrender_buffer_icon);

    struct hashset_iterator iterator_is_in_desktops = { };
    struct desktop *desktop = NULL;
    hashset_iterator_init(&iterator_is_in_desktops, &toplevel_handle->is_in_desktops);
    while ((desktop = hashset_iterator_next_deletion(&iterator_is_in_desktops)));
    hashset_cleanup(&toplevel_handle->is_in_desktops);

    free(toplevel_handle);
}

static void toplevel_handle_manage_icon_loading(struct toplevel_handle *toplevel_handle)
{
    if (!toplevel_handle->app_id_changed || !toplevel_handle->app_id)
        return;

    // TODO: implement toplevel_handle icon loading...
    SWRender_BufferFree(&toplevel_handle->swrender_buffer_icon);

    char *path = NULL;
    SWRenderBuffer swrender_buffer_new_icon = { };

    icon_find_app_icon_path(toplevel_handle->app_id, 64, 1, &path);

    if (!path)
        return;

    if (SWRender_LoadImageFromFileToBuffer(path, &swrender_buffer_new_icon) < 0)
        fprintf(stderr, "failed to load icon '%s' into buffer: %s\n", path, strerror(errno));
    else
    {
        toplevel_handle->swrender_buffer_icon.w = DESKTOP_ICON_DEFAULT_ICON_SIZE_X;
        toplevel_handle->swrender_buffer_icon.h = DESKTOP_ICON_DEFAULT_ICON_SIZE_Y;
        SWRender_DupSWScaledToBuffer(&swrender_buffer_new_icon, &toplevel_handle->swrender_buffer_icon);
        SWRender_BufferFree(&swrender_buffer_new_icon);
    }

    debug_printf(
        "icon path for '%s' (%s): %s\n",
        debug_str_nullable(toplevel_handle->title),
        debug_str_nullable(toplevel_handle->app_id),
        path
    );

    free(path);

    toplevel_handle->icon_changed = true;
}

static void toplevel_handle_manage_post_update(struct toplevel_handle *toplevel_handle)
{
    toplevel_handle->title_changed  = false;
    toplevel_handle->app_id_changed = false;

    toplevel_handle->minimized_changed = false;

    toplevel_handle->desktop_changed = false;
}

#pragma endregion

#pragma region toplevel_handles

void toplevel_handles_init(void)
{
    hashset_init(&toplevel_handles);
}

void toplevel_handles_cleanup(void)
{
    struct hashset_iterator iterator_toplevel_handles = { };
    struct toplevel_handle *toplevel_handle = NULL;
    hashset_iterator_init(&iterator_toplevel_handles, &toplevel_handles);
    while ((toplevel_handle = hashset_iterator_next_deletion(&iterator_toplevel_handles)))
        toplevel_handle_destroy(toplevel_handle);
    hashset_cleanup(&toplevel_handles);
}

static void toplevel_handles_add_toplevel_handle(struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle)
{
    struct toplevel_handle *toplevel_handle = toplevel_handle_create(zwlr_foreign_toplevel_handle);

    struct toplevel_handle *overwritten_toplevel_handle = NULL;
    hashset_insert(&toplevel_handles, toplevel_handle, (void **)&overwritten_toplevel_handle);
    assert(overwritten_toplevel_handle == NULL);

    // desktop icon
    struct hashset_iterator iterator_desktops = { };
    struct desktop *desktop = NULL;
    hashset_iterator_init(&iterator_desktops, &desktops);
    while ((desktop = hashset_iterator_next(&iterator_desktops)))
    {
        if (!desktop->icon_loaded)
            continue;

        struct desktop_icon *desktop_icon = desktop_icon_create(toplevel_handle, desktop);
        struct linked_list_node *node_desktop_icon = linked_list_node_create(desktop_icon);
        linked_list_insert_node_tail(&desktop->desktop_icons, node_desktop_icon);
    }
}

void toplevel_handles_manage_icon_loading(void)
{
    struct hashset_iterator iterator_toplevel_handles = { };
    struct toplevel_handle *toplevel_handle = NULL;
    hashset_iterator_init(&iterator_toplevel_handles, &toplevel_handles);
    while ((toplevel_handle = hashset_iterator_next(&iterator_toplevel_handles)))
        toplevel_handle_manage_icon_loading(toplevel_handle);
}

void toplevel_handles_manage_post_update(void)
{
    struct hashset_iterator iterator_toplevel_handles = { };
    struct toplevel_handle *toplevel_handle = NULL;
    hashset_iterator_init(&iterator_toplevel_handles, &toplevel_handles);
    while ((toplevel_handle = hashset_iterator_next(&iterator_toplevel_handles)))
    {
        toplevel_handle_manage_post_update(toplevel_handle);

        if (!toplevel_handle->closed)
            continue;

        struct linked_list_node *node_toplevel_handle_to_remove = linked_list_node_create(toplevel_handle);
        linked_list_insert_node_tail(&toplevel_handles_to_remove, node_toplevel_handle_to_remove);
    }
}

void toplevel_handles_manage_closed(void)
{
    struct linked_list_iterator iterator_toplevel_handles_to_remove = { };
    struct linked_list_node *node_toplevel_handle_to_remove = NULL;
    linked_list_iterator_init(&iterator_toplevel_handles_to_remove, &toplevel_handles_to_remove);
    while ((node_toplevel_handle_to_remove = linked_list_iterator_next_detaching(&iterator_toplevel_handles_to_remove)))
    {
        hashset_delete(&toplevel_handles, node_toplevel_handle_to_remove->elem);
        toplevel_handle_destroy(node_toplevel_handle_to_remove->elem);
        linked_list_node_destroy(node_toplevel_handle_to_remove);
    }
}

#pragma endregion

#pragma region foreign_toplevel_manager

#pragma region foreign_toplevel_manager_listeners

static void foreign_toplevel_manager_toplevel(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager_v1,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_foreign_toplevel_handle
)
{
    toplevel_handles_add_toplevel_handle(zwlr_foreign_toplevel_handle);
}

static void foreign_toplevel_manager_finished(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager_v1
)
{
    is_foreign_toplevel_manager_finished = true;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener foreign_toplevel_manager_listener =
{
    .toplevel = foreign_toplevel_manager_toplevel,
    .finished = foreign_toplevel_manager_finished,
};

#pragma endregion

void foreign_toplevel_manager_init(struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager_v1)
{
    assert(zwlr_foreign_toplevel_manager_v1 != NULL);
    zwlr_foreign_toplevel_manager = zwlr_foreign_toplevel_manager_v1;
    zwlr_foreign_toplevel_manager_v1_add_listener(zwlr_foreign_toplevel_manager, &foreign_toplevel_manager_listener, NULL);
}

void foreign_toplevel_manager_cleanup(void)
{
    zwlr_foreign_toplevel_manager_v1_destroy(zwlr_foreign_toplevel_manager);
}

#pragma endregion

#pragma region desktop

#pragma region desktop_hash_functions

static size_t desktop_hashset_hash_function(void *ptr)
{
    struct desktop *desktop = ptr;
    return desktop->wl_output_name;
}

static bool desktop_hashset_is_equal(void *l, void *r)
{
    struct desktop *desktop_l = l;
    struct desktop *desktop_r = r;
    return (desktop_l->wl_output_name == desktop_r->wl_output_name);
}

#pragma endregion

#pragma region desktop_output_listeners

static void desktop_output_geometry(
    void *data,
    struct wl_output *wl_output,
    int32_t x,
    int32_t y,
    int32_t physical_width,
    int32_t physical_height,
    int32_t subpixel,
    const char *make,
    const char *model,
    int32_t transform
)
{
    struct desktop *desktop = data;

    desktop->output_transform = transform;
}

static void desktop_output_mode(
    void *data,
    struct wl_output *wl_output,
    uint32_t flags,
    int32_t width,
    int32_t height,
    int32_t refresh
)
{
    struct desktop *desktop = data;

    desktop->output_w_original = width;
    desktop->output_h_original = height;
}

static void desktop_output_done(
    void *data,
    struct wl_output *wl_output
)
{
    struct desktop *desktop = data;

    switch (desktop->output_transform)
    {
        default:
        case WL_OUTPUT_TRANSFORM_NORMAL:
        case WL_OUTPUT_TRANSFORM_180:
        case WL_OUTPUT_TRANSFORM_FLIPPED:
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            desktop->output_w = desktop->output_w_original;
            desktop->output_h = desktop->output_h_original;
            break;
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            desktop->output_w = desktop->output_h_original;
            desktop->output_h = desktop->output_w_original;
            break;
    }

    desktop->has_valid_output_mode = true;
}

static void desktop_output_scale(
    void *data,
    struct wl_output *wl_output,
    int32_t factor
)
{
    // currently left blank (unused)
    // TODO: scale...?
}

static void desktop_output_name(
    void *data,
    struct wl_output *wl_output,
    const char *name
)
{
    struct desktop *desktop = data;

    if (desktop->output_name)
        free(desktop->output_name);

    desktop->output_name = strdup(name);

    debug_printf("output: %s\n", debug_str_nullable(desktop->output_name));
}

static void desktop_output_description(
    void *data,
    struct wl_output *wl_output,
    const char *description
)
{
    // currently left blank (unused)
}

static const struct wl_output_listener desktop_output_listener =
{
    .geometry    = desktop_output_geometry,
    .mode        = desktop_output_mode,
    .done        = desktop_output_done,
    .scale       = desktop_output_scale,
    .name        = desktop_output_name,
    .description = desktop_output_description,
};

#pragma endregion

#pragma region desktop_layer_surface_listeners

static void desktop_layer_surface_configure(
    void *data,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
    uint32_t serial,
    uint32_t width,
    uint32_t height
)
{
    struct desktop *desktop = data;

    if (width == 0)
        width = desktop->output_w;
    if (height == 0)
        height = desktop->output_h;

    zwlr_layer_surface_v1_set_size(desktop->zwlr_layer_surface, width, height);

    double_buffer_surface_set_size(&desktop->double_buffer_surface, width, height);
    vector_insert(&desktop->damaged_regions, &(SWRenderBoundI){ 0, 0, width - 1, height - 1 });

    //if (!desktop->wp_viewport)
    //    desktop->wp_viewport = wp_viewporter_get_viewport(wp_viewporter, desktop->double_buffer_surface.wl_surface);
    //wp_viewport_set_destination(desktop->wp_viewport, (int32_t)width, (int32_t)height);

    zwlr_layer_surface_v1_ack_configure(desktop->zwlr_layer_surface, serial);

    debug_printf(
        "desktop of output '%s' layer surface configure w=%u, h=%u\n",
        debug_str_nullable(desktop->output_name),
        width, height
    );
}

static void desktop_layer_surface_closed(
    void *data,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1
)
{
    struct desktop *desktop = data;

    desktop->removed = true;
}

static const struct zwlr_layer_surface_v1_listener desktop_layer_surface_listener =
{
    .configure = desktop_layer_surface_configure,
    .closed    = desktop_layer_surface_closed,
};

#pragma endregion

static struct desktop *desktop_create(struct wl_output *wl_output, uint32_t name)
{
    assert(wl_output != NULL);

    struct desktop *desktop = calloc(1, sizeof(*desktop));
    assert(desktop != NULL);

    desktop->wl_output = wl_output;
    desktop->wl_output_name = name;
    wl_output_add_listener(wl_output, &desktop_output_listener, desktop);

    double_buffer_surface_init(&desktop->double_buffer_surface);
    // wait until layer shell exists and layer surface created
    desktop->no_layer_surface_yet = true;

    vector_init(&desktop->damaged_regions, sizeof(SWRenderBoundI));

    // TODO: make reconfigurable
    desktop->icon_w = DESKTOP_ICON_DEFAULT_W;
    desktop->icon_h = DESKTOP_ICON_DEFAULT_H;
    desktop->icon_corner_radius = DESKTOP_ICON_DEFAULT_CORNER_RADIUS;
    desktop->icon_spacing = DESKTOP_ICON_DEFAULT_SPACING;

    desktop->icon_title_font_size = DESKTOP_ICON_DEFAULT_TEXT_SIZE;

    return desktop;
}

static void desktop_destroy(struct desktop *desktop)
{
    assert(desktop != NULL);

    //if (desktop->wp_viewport)
    //    wp_viewport_destroy(desktop->wp_viewport);
    if (desktop->zwlr_layer_surface)
        zwlr_layer_surface_v1_destroy(desktop->zwlr_layer_surface);
    double_buffer_surface_cleanup(&desktop->double_buffer_surface);

    vector_cleanup(&desktop->damaged_regions);

    struct linked_list_iterator iterator_desktop_icons = { };
    struct linked_list_node *desktop_icon_node = NULL;
    linked_list_iterator_init(&iterator_desktop_icons, &desktop->desktop_icons);
    while ((desktop_icon_node = linked_list_iterator_next_detaching(&iterator_desktop_icons)))
    {
        desktop_icon_destroy(desktop_icon_node->elem);
        linked_list_node_destroy(desktop_icon_node);
    }

    if (desktop->output_name)
        free(desktop->output_name);

    wl_output_destroy(desktop->wl_output);

    free(desktop);
}

static void desktop_reset_regions_to_redraw(struct desktop *desktop)
{
    if (desktop->no_layer_surface_yet)
        return;

    desktop->region_to_redraw = (SWRenderBoundI){
        desktop->double_buffer_surface.w,
        desktop->double_buffer_surface.h,
        -1,
        -1,
    };
}

static void desktop_manage_toplevel_handles_loading(struct desktop *desktop)
{
    if (desktop->removed)
        return;

    if (desktop->icon_loaded)
        return;

    struct hashset_iterator iterator_toplevel_handles = { };
    struct toplevel_handle *toplevel_handle = NULL;
    hashset_iterator_init(&iterator_toplevel_handles, &toplevel_handles);
    while ((toplevel_handle = hashset_iterator_next(&iterator_toplevel_handles)))
    {
        struct desktop_icon *desktop_icon = desktop_icon_create(toplevel_handle, desktop);
        struct linked_list_node *node_desktop_icon = linked_list_node_create(desktop_icon);
        linked_list_insert_node_tail(&desktop->desktop_icons, node_desktop_icon);
    }

    desktop->icon_loaded = true;
}

static void desktop_manage_layer_surface_creation(struct desktop *desktop)
{
    if (desktop->removed)
        return;

    if (!desktop->no_layer_surface_yet || !desktop->has_valid_output_mode || !zwlr_layer_shell)
        return;

    desktop->zwlr_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        zwlr_layer_shell,
        desktop->double_buffer_surface.wl_surface,
        desktop->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        "desktop"
    );

    zwlr_layer_surface_v1_set_size(
        desktop->zwlr_layer_surface,
        0, 0
    );
    zwlr_layer_surface_v1_set_anchor(
        desktop->zwlr_layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
    );
    zwlr_layer_surface_v1_set_margin(
        desktop->zwlr_layer_surface,
        0, 0, 0, 0
    );
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        desktop->zwlr_layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND
    );

    zwlr_layer_surface_v1_add_listener(desktop->zwlr_layer_surface, &desktop_layer_surface_listener, desktop);

    wl_surface_commit(desktop->double_buffer_surface.wl_surface);

    desktop->no_layer_surface_yet = false;
}

static void desktop_manage_input_events(struct desktop *desktop)
{
    bool any_keyboard_inside = false;

    struct hashset_iterator iterator_keyboard_states = { };
    struct hashset_iterator iterator_pointer_states = { };
    struct keyboard_state *keyboard_state = NULL;
    struct pointer_state *pointer_state = NULL;

    // pointer - cursor shape
    hashset_iterator_init(&iterator_pointer_states, &desktop->double_buffer_surface.pointer_states);
    while ((pointer_state = hashset_iterator_next(&iterator_pointer_states)))
    {
        if (pointer_state->pointer_inside)
            pointer_state_set_cursor(pointer_state, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    }

    // keyboard - shared
    hashset_iterator_init(&iterator_keyboard_states, &desktop->double_buffer_surface.keyboard_states);
    while ((keyboard_state = hashset_iterator_next(&iterator_keyboard_states)))
    {
        if (keyboard_state->keyboard_focused)
        {
            any_keyboard_inside = true;
            break;
        }
    }
    if (!any_keyboard_inside)
    {
        desktop->is_operating_by_pointer_state = NULL;
        desktop->selected_desktop_icon = NULL;
        return;
    }
    
    // pointer - preemptive
    // drag
    if (desktop->is_operating_by_pointer_state && desktop->selected_desktop_icon)
    {
        struct pointer_button_state *lmb_state = pointer_state_get_button_state(desktop->is_operating_by_pointer_state, BTN_LEFT);
        struct pointer_button_state *mmb_state = pointer_state_get_button_state(desktop->is_operating_by_pointer_state, BTN_MIDDLE);

        if (lmb_state->drag_started)
        {
            desktop->icon_drag_start_x = desktop->selected_desktop_icon->geometry.x;
            desktop->icon_drag_start_y = desktop->selected_desktop_icon->geometry.y;

            desktop->selected_desktop_icon->recalculate_position_when_minimize = false;
        }

        if (lmb_state->is_dragging)
        {
            int pointer_drag_distance_x = desktop->is_operating_by_pointer_state->x_int - lmb_state->drag_start_x_int;
            int pointer_drag_distance_y = desktop->is_operating_by_pointer_state->y_int - lmb_state->drag_start_y_int;
            desktop->selected_desktop_icon->geometry.x = desktop->icon_drag_start_x + pointer_drag_distance_x;
            desktop->selected_desktop_icon->geometry.y = desktop->icon_drag_start_y + pointer_drag_distance_y;

            int min_x = -desktop->icon_w / 2;
            int min_y = -desktop->icon_h / 2;
            int max_x = desktop->double_buffer_surface.w + min_x - 1;
            int max_y = desktop->double_buffer_surface.h + min_y - 1;
            if (desktop->selected_desktop_icon->geometry.x < min_x)
                desktop->selected_desktop_icon->geometry.x = min_x;
            if (desktop->selected_desktop_icon->geometry.x > max_x)
                desktop->selected_desktop_icon->geometry.x = max_x;
            if (desktop->selected_desktop_icon->geometry.y < min_y)
                desktop->selected_desktop_icon->geometry.y = min_y;
            if (desktop->selected_desktop_icon->geometry.y > max_y)
                desktop->selected_desktop_icon->geometry.y = max_y;
        }
    }

    // clicks
    struct desktop_icon *desktop_icon_to_open  = NULL;
    struct desktop_icon *desktop_icon_to_close = NULL;
    bool lmb_selected_icon = false;

    hashset_iterator_init(&iterator_pointer_states, &desktop->double_buffer_surface.pointer_states);
    while ((pointer_state = hashset_iterator_next(&iterator_pointer_states)))
    {
        if (pointer_state->seat->pointer.removed && desktop->is_operating_by_pointer_state == pointer_state)
        {
            desktop->is_operating_by_pointer_state = NULL;
            desktop->selected_desktop_icon = NULL;
            continue;
        }

        struct pointer_button_state *lmb_state = pointer_state_get_button_state(pointer_state, BTN_LEFT);
        struct pointer_button_state *mmb_state = pointer_state_get_button_state(pointer_state, BTN_MIDDLE);

        struct linked_list_iterator iterator_desktop_icons = { };
        struct linked_list_node *desktop_icon_node = NULL;
        linked_list_iterator_init(&iterator_desktop_icons, &desktop->desktop_icons);
        while ((desktop_icon_node = linked_list_iterator_next(&iterator_desktop_icons)))
        {
            struct desktop_icon *desktop_icon = desktop_icon_node->elem;

            if (!desktop_icon_geometry_is_visible(&desktop_icon->geometry))
                continue;

            bool mmb_press_on_icon   = mmb_state->pressed && desktop_icon_geometry_is_position_inside(&desktop_icon->geometry, desktop, mmb_state->press_x_int, mmb_state->press_y_int);
            bool lmb_press_on_icon   = lmb_state->pressed && desktop_icon_geometry_is_position_inside(&desktop_icon->geometry, desktop, lmb_state->press_x_int, lmb_state->press_y_int);
            bool mmb_release_on_icon = mmb_state->released && desktop_icon_geometry_is_position_inside(&desktop_icon->geometry, desktop, mmb_state->release_x_int, mmb_state->release_y_int);

            if (lmb_press_on_icon || mmb_press_on_icon)
            {
                desktop->selected_desktop_icon         = desktop_icon;
                desktop->is_operating_by_pointer_state = pointer_state;
                lmb_selected_icon = true;
            }
            if (lmb_state->double_clicked && lmb_press_on_icon)
                desktop_icon_to_open  = desktop_icon;
            if (mmb_release_on_icon && (desktop_icon == desktop->selected_desktop_icon))
                desktop_icon_to_close = desktop_icon;
        }
    }

    if (lmb_selected_icon && desktop->selected_desktop_icon)
        desktop->selected_desktop_icon->send_to_front = true;
    if (desktop_icon_to_open)
        zwlr_foreign_toplevel_handle_v1_unset_minimized(desktop_icon_to_open->toplevel_handle->zwlr_foreign_toplevel_handle);
    if (desktop_icon_to_close)
        zwlr_foreign_toplevel_handle_v1_close(desktop_icon_to_close->toplevel_handle->zwlr_foreign_toplevel_handle);
}

static void desktop_manage_desktop_icon_state_updates(struct desktop *desktop)
{
    struct linked_list desktop_icons_to_sent_front = { };
    bool has_buffer_available = double_buffer_surface_is_any_buffer_available(&desktop->double_buffer_surface);

    struct linked_list_iterator iterator_desktop_icons = { };
    struct linked_list_node *desktop_icon_node = NULL;
    linked_list_iterator_init(&iterator_desktop_icons, &desktop->desktop_icons);
    while ((desktop_icon_node = linked_list_iterator_next(&iterator_desktop_icons)))
    {
        struct desktop_icon *desktop_icon = desktop_icon_node->elem;
        
        desktop_icon_manage_may_pend_redraw_content(desktop_icon, desktop);
        
        desktop_icon_manage_visibility(desktop_icon, desktop);
        desktop_icon_manage_may_pend_send_to_front(desktop_icon, desktop);

        desktop_icon_manage_position_may_recalculation(desktop_icon, desktop);

        if (has_buffer_available)
            desktop_icon_may_render_content(desktop_icon, desktop);

        if (desktop_icon->send_to_front)
        {
            struct linked_list_node *node = linked_list_node_create(desktop_icon_node);
            linked_list_insert_node_tail(&desktop_icons_to_sent_front, node);
            desktop_icon->send_to_front = false;
        }
    }

    struct linked_list_iterator iterator_desktop_icons_to_sent_front = { };
    struct linked_list_node *desktop_icon_to_sent_front_node_node = NULL;
    linked_list_iterator_init(&iterator_desktop_icons_to_sent_front, &desktop_icons_to_sent_front);
    while ((desktop_icon_to_sent_front_node_node = linked_list_iterator_next_detaching(&iterator_desktop_icons_to_sent_front)))
    {
        struct linked_list_node *desktop_icon_to_sent_front_node = desktop_icon_to_sent_front_node_node->elem;
        linked_list_node_detach(desktop_icon_to_sent_front_node);
        linked_list_insert_node_tail(&desktop->desktop_icons, desktop_icon_to_sent_front_node);
        linked_list_node_destroy(desktop_icon_to_sent_front_node_node);
    }
}

static void desktop_consume_render_flags_and_calculate_redraw_region(struct desktop *desktop)
{
    struct linked_list_iterator iterator_desktop_icons = { };
    struct linked_list_node *desktop_icon_node = NULL;
    linked_list_iterator_init(&iterator_desktop_icons, &desktop->desktop_icons);
    while ((desktop_icon_node = linked_list_iterator_next(&iterator_desktop_icons)))
    {
        struct desktop_icon *desktop_icon = desktop_icon_node->elem;

        if (!desktop_icon->geometry.valid)
        {
            desktop_icon->geometry_last_render = desktop_icon->geometry;
            continue;
        }
        
        bool last_geometry_visible = desktop_icon_geometry_is_visible(&desktop_icon->geometry_last_render);
        bool curr_geometry_visible = desktop_icon_geometry_is_visible(&desktop_icon->geometry);
        bool visibility_changed = (last_geometry_visible != curr_geometry_visible);

        // TODO: transition from last valid to curr invalid?

        if (!desktop_icon_geometry_is_position_equal(&desktop_icon->geometry_last_render, &desktop_icon->geometry))
        {
            if (last_geometry_visible)
                desktop_icon_geometry_damage_desktop(&desktop_icon->geometry_last_render, desktop);
            if (curr_geometry_visible)
                desktop_icon_geometry_damage_desktop(&desktop_icon->geometry, desktop);
        }
        else if (visibility_changed)
        {
            desktop_icon_geometry_damage_desktop(&desktop_icon->geometry, desktop);
            debug_printf(
                "'%s' (%s) visibility changed %d -> %d\n",
                debug_str_nullable(desktop_icon->toplevel_handle->title),
                debug_str_nullable(desktop_icon->toplevel_handle->app_id),
                last_geometry_visible, curr_geometry_visible
            );
        }
        else
        {
            if (desktop_icon->redraw && curr_geometry_visible)
                desktop_icon_geometry_damage_desktop(&desktop_icon->geometry, desktop);
        }

        desktop_icon->geometry_last_render = desktop_icon->geometry;
        desktop_icon->redraw = false;
    }
}

static void desktop_manage_input_region(struct desktop *desktop)
{
    struct wl_region *input_region = wl_compositor_create_region(wl_compositor);

    struct linked_list_iterator iterator_desktop_icons = { };
    struct linked_list_node *desktop_icon_node = NULL;
    linked_list_iterator_init(&iterator_desktop_icons, &desktop->desktop_icons);
    while ((desktop_icon_node = linked_list_iterator_next(&iterator_desktop_icons)))
    {
        struct desktop_icon *desktop_icon = desktop_icon_node->elem;

        if (!desktop_icon_geometry_is_visible(&desktop_icon->geometry))
            continue;

        wl_region_add(
            input_region,
            desktop_icon->geometry.x,
            desktop_icon->geometry.y,
            desktop->icon_w,
            desktop->icon_h
        );
    }

    wl_surface_set_input_region(desktop->double_buffer_surface.wl_surface, input_region);
    wl_region_destroy(input_region);
}

static void desktop_render_redraw_region(struct desktop *desktop)
{
    if (desktop->damaged_regions.count == 0)
        return;

    SWRenderBuffer swrender_buffer_region_to_redraw;

    // calculate region_to_redraw
    desktop_reset_regions_to_redraw(desktop);

    struct vector_iterator iterator_damaged_regions = { };
    SWRenderBoundI *damaged_region = NULL;
    vector_iterator_init(&iterator_damaged_regions, &desktop->damaged_regions);
    while ((damaged_region = vector_iterator_next(&iterator_damaged_regions)))
        SWRender_IncludeBoundToBoundInt(&desktop->region_to_redraw, damaged_region);

    vector_clear(&desktop->damaged_regions);

    // ignore invalid
    if ((desktop->region_to_redraw.x2 < desktop->region_to_redraw.x1) || (desktop->region_to_redraw.y2 < desktop->region_to_redraw.y1))
        return;

    // render only parts inside screen to avoid problems caused by negative coordinates
    SWRenderBoundI swrender_buffer_bound;
    SWRender_BufferGetBounds(&swrender_buffer_bound, &desktop->double_buffer_surface.swrender_buffer);
    SWRender_GetIntersectionBetweenBoundInt(&desktop->region_to_redraw, &swrender_buffer_bound, &desktop->region_to_redraw);

    // ignore this render if outside screen
    if (!SWRender_BufferCrop(&swrender_buffer_region_to_redraw, &desktop->double_buffer_surface.swrender_buffer, &desktop->region_to_redraw))
        return;

    double_buffer_surface_damage(&desktop->double_buffer_surface, &(struct buffer_rect){
        desktop->region_to_redraw.x1,
        desktop->region_to_redraw.y1,
        desktop->region_to_redraw.x2 - desktop->region_to_redraw.x1 + 1,
        desktop->region_to_redraw.y2 - desktop->region_to_redraw.y1 + 1,
    });
    desktop->damaged = true;

    // ------------------------ do redraw things here -------------------------

    // clear background to transparent
    SWRender_FillRect(&swrender_buffer_region_to_redraw, NULL, (SWRenderColor){ }, true);

    struct linked_list_iterator iterator_desktop_icons = { };
    struct linked_list_node *desktop_icon_node = NULL;
    linked_list_iterator_init(&iterator_desktop_icons, &desktop->desktop_icons);
    while ((desktop_icon_node = linked_list_iterator_next(&iterator_desktop_icons)))
    {
        struct desktop_icon *desktop_icon = desktop_icon_node->elem;

        if (!desktop_icon_geometry_is_visible(&desktop_icon->geometry))
            continue;

        SWRenderVec2I icon_local_render_pos =
        {
            -desktop->region_to_redraw.x1 + desktop_icon->geometry.x + desktop_icon->geometry.draw_origin_x,
            -desktop->region_to_redraw.y1 + desktop_icon->geometry.y + desktop_icon->geometry.draw_origin_y,
        };
        SWRender_CopyBuffer(&desktop_icon->swrender_buffer_content, NULL, &swrender_buffer_region_to_redraw, icon_local_render_pos);
    }
}

static void desktop_surface_commit(struct desktop *desktop)
{
    if (desktop->damaged && double_buffer_surface_try_commit(&desktop->double_buffer_surface))
        desktop->damaged = false;
}

static void desktop_manage_post_update(struct desktop *desktop)
{
    // TODO: desktop post update?
}

static void desktop_manage_desktop_icon_removal(struct desktop *desktop)
{
    struct linked_list desktop_icons_to_remove = { };
    struct linked_list_node *desktop_icon_node = NULL;
    struct linked_list_node *desktop_icon_node_node = NULL;

    struct linked_list_iterator iterator_desktop_icons = { };
    linked_list_iterator_init(&iterator_desktop_icons, &desktop->desktop_icons);
    while ((desktop_icon_node = linked_list_iterator_next(&iterator_desktop_icons)))
    {
        struct desktop_icon *desktop_icon = desktop_icon_node->elem;

        if (!desktop_icon->toplevel_handle->closed)
            continue;

        struct linked_list_node *node = linked_list_node_create(desktop_icon_node);
        linked_list_insert_node_tail(&desktop_icons_to_remove, node);
    }

    struct linked_list_iterator iterator_desktop_icons_to_remove = { };
    linked_list_iterator_init(&iterator_desktop_icons_to_remove, &desktop_icons_to_remove);
    while ((desktop_icon_node_node = linked_list_iterator_next_detaching(&iterator_desktop_icons_to_remove)))
    {
        desktop_icon_node = desktop_icon_node_node->elem;
        linked_list_node_detach(desktop_icon_node);
        desktop_icon_destroy(desktop_icon_node->elem);
        linked_list_node_destroy(desktop_icon_node);
        linked_list_node_destroy(desktop_icon_node_node);
    }
}

#pragma endregion

#pragma region desktops

void desktops_init(void)
{
    hashset_init(&desktops);
    desktops.hash_function = desktop_hashset_hash_function;
    desktops.is_equal = desktop_hashset_is_equal;
}

void desktops_cleanup(void)
{
    struct hashset_iterator iterator_desktops = { };
    struct desktop *desktop = NULL;
    hashset_iterator_init(&iterator_desktops, &desktops);
    while ((desktop = hashset_iterator_next_deletion(&iterator_desktops)))
        desktop_destroy(desktop);
    hashset_cleanup(&desktops);
}

void desktops_add_by_wl_output(struct wl_output *wl_output, uint32_t name)
{
    struct desktop *desktop = desktop_create(wl_output, name);
    struct desktop *overwritten_desktop = NULL;
    hashset_insert(&desktops, desktop, (void **)&overwritten_desktop);
    assert(overwritten_desktop == NULL);
}

void desktops_pend_remove_by_name(uint32_t name)
{
    struct desktop desktop_to_find = { .wl_output_name = name };
    struct desktop *desktop_to_remove = hashset_find(&desktops, &desktop_to_find);
    if (!desktop_to_remove)
        return;
       
    desktop_to_remove->removed = true;

    struct linked_list_node *node_desktop_to_remove = linked_list_node_create(desktop_to_remove);
    linked_list_insert_node_tail(&desktops_to_remove, node_desktop_to_remove);
}

void desktops_manage_creation(void)
{
    struct hashset_iterator iterator_desktops = { };
    struct desktop *desktop = NULL;

    hashset_iterator_init(&iterator_desktops, &desktops);
    while ((desktop = hashset_iterator_next(&iterator_desktops)))
    {
        if (desktop->removed)
            continue;

        desktop_manage_layer_surface_creation(desktop);
        desktop_manage_toplevel_handles_loading(desktop);
    }
}

void desktops_manage(void)
{
    struct hashset_iterator iterator_desktops = { };
    struct desktop *desktop = NULL;

    hashset_iterator_init(&iterator_desktops, &desktops);
    while ((desktop = hashset_iterator_next(&iterator_desktops)))
    {
        if (desktop->removed || desktop->no_layer_surface_yet || !desktop->double_buffer_surface.valid)
            continue;

        desktop_manage_input_events(desktop);

        desktop_manage_desktop_icon_state_updates(desktop);

        if (double_buffer_surface_is_any_buffer_available(&desktop->double_buffer_surface))
        {
            desktop_consume_render_flags_and_calculate_redraw_region(desktop);

            desktop_render_redraw_region(desktop);

            desktop_surface_commit(desktop);
        }

        desktop_manage_input_region(desktop);

        desktop_manage_desktop_icon_removal(desktop);
    }
}

void desktops_manage_post_update(void)
{
    // TODO: implement desktops_manage_post_update
}

void desktops_manage_removal(void)
{
    struct linked_list_iterator iterator_desktops_to_remove = { };
    struct linked_list_node *desktop_to_remove_node = NULL;
    struct desktop *desktop_to_remove = NULL;
    linked_list_iterator_init(&iterator_desktops_to_remove, &desktops_to_remove);
    while ((desktop_to_remove_node = linked_list_iterator_next_detaching(&iterator_desktops_to_remove)))
    {
        desktop_to_remove = desktop_to_remove_node->elem;
        hashset_delete(&desktops, desktop_to_remove);
        desktop_destroy(desktop_to_remove);
        linked_list_node_destroy(desktop_to_remove_node);
    }
}

#pragma endregion
