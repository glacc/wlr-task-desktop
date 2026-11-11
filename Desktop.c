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

#include "Desktop.h"

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <malloc.h>
#include <limits.h>

#include "Utility.h"

#include "ToplevelHandle.h"

#include "Globals.h"

#include "SWRender/SWRender.h"
#include "SWRender/FTEssentials.h"
#include "SWRender/FTRenderWordWrap.h"

int Desktop_debug_verbosity = 1;

#pragma region WaylandListener

static void Desktop_OutputGeometry(
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
    struct Desktop *desktop = data;

    desktop->x = x;
    desktop->y = y;
    desktop->phy_w = physical_width;
    desktop->phy_h = physical_height;

    if (Desktop_debug_verbosity < 1)
        return;

    printf(
        "geometry %s %s: %dx%d at (%d, %d)\n",
        make, model, physical_width, physical_height, x, y
    );
}

static void Desktop_OutputMode(
    void *data,
    struct wl_output *wl_output,
    uint32_t flags,
    int32_t width,
    int32_t height,
    int32_t refresh
)
{
    struct Desktop *desktop = data;

    desktop->w = width;
    desktop->h = height;

    if (Desktop_debug_verbosity < 1)
        return;

    printf("\tmode: %dx%d@%d.%03dHz\n", width, height, refresh / 1000, refresh % 1000);
}

static void Desktop_OutputDone(
    void *data,
    struct wl_output *wl_output
)
{
    struct Desktop *desktop = data;

    desktop->wl_output = wl_output;

    if (!desktop->scale)
        desktop->scale = 1;

    if (Desktop_debug_verbosity < 1)
        return;

    printf("\twl_name: %u\n", desktop->wl_name);

    printf("output done\n");
}

static void Desktop_OutputScale(
    void *data,
    struct wl_output *wl_output,
    int32_t factor
)
{
    struct Desktop *desktop = data;

    desktop->scale = factor;

    if (Desktop_debug_verbosity < 1)
        return;

    printf("\tscale: %d\n", factor);
}

static void Desktop_OutputName(
    void *data,
    struct wl_output *wl_output,
    const char *name
)
{
    struct Desktop *desktop = data;
    Utility_TryReplaceWithAnotherString(&desktop->name, name);

    if (Desktop_debug_verbosity < 1)
        return;

    printf("\tname: %s\n", name);
}

static void Desktop_OutputDescription(
    void *data,
    struct wl_output *wl_output,
    const char *description
)
{
    struct Desktop *desktop = data;

    Utility_TryReplaceWithAnotherString(&desktop->description, description);

    if (Desktop_debug_verbosity < 1)
        return;

    printf("\tdescription: %s\n", description);
}

const struct wl_output_listener Desktop_output_wl_listener =
{
    .geometry = Desktop_OutputGeometry,
    .mode = Desktop_OutputMode,
    .done = Desktop_OutputDone,
    .scale = Desktop_OutputScale,
    .name = Desktop_OutputName,
    .description = Desktop_OutputDescription,
};

#pragma endregion

size_t Desktop_Hash(void *ptr)
{
    struct Desktop *desktop = ptr;
    return desktop->wl_name;
}

bool Desktop_IsEqual(void *l, void *r)
{
    struct Desktop *desktop_l = l;
    struct Desktop *desktop_r = r;
    return (desktop_l->wl_name == desktop_r->wl_name);
}

struct Desktop *Desktop_Create(void)
{
    struct Desktop *desktop = calloc(1, sizeof(*desktop));

    desktop->damaged = false;

    HashSet_Init(&desktop->toplevel_handles);
    Vector_Init(&desktop->damage_rect_bounds, sizeof(SWRenderBoundI));

    return desktop;
}

void Desktop_Destroy(struct Desktop *desktop)
{
    if (!desktop)
        return;

    // free strings
    if (desktop->name)
        free(desktop->name);
    if (desktop->description)
        free(desktop->description);

    struct HashSet_Iter iter = { };

    // unref ToplevelHandles
    if (HashSet_IterInit(&iter, &desktop->toplevel_handles))
    {
        struct ToplevelHandle *toplevel_handle = NULL;

        while ((toplevel_handle = HashSet_IterDel(&iter)))
        {
            HashSet_Del(&toplevel_handle->is_inside_desktops, desktop);

            // TODO: guessing main display by earilest name value?

            // change iconify_to_desktop if == this desktop
            if (toplevel_handle->iconify_to_desktop == desktop)
            {
                struct HashSet_Iter iter_toplevel_handle_output = { };

                if (HashSet_IterInit(&iter_toplevel_handle_output, &toplevel_handle->is_inside_desktops))
                    toplevel_handle->iconify_to_desktop = HashSet_IterNext(&iter_toplevel_handle_output);
                else
                    toplevel_handle->iconify_to_desktop = NULL;
            }
        }
    }
    HashSet_Cleanup(&desktop->toplevel_handles);
    
    Vector_Cleanup(&desktop->damage_rect_bounds);

    DesktopLayerSurface_Cleanup(&desktop->desktop_layer_surface);

    // destory wl_output object
    if (desktop->wl_output)
        wl_output_destroy(desktop->wl_output);

    free(desktop);
}

void Desktop_Update(struct Desktop *desktop)
{
    if (!desktop)
        return;

    struct PointerState *pointer_state = &desktop->desktop_layer_surface.double_buffered_surface.pointer_state;
    struct PointerBtnState *lmb_state = Pointer_GetBtnState(pointer_state, 272);
    struct PointerBtnState *rmb_state = Pointer_GetBtnState(pointer_state, 273);
    struct PointerBtnState *mmb_state = Pointer_GetBtnState(pointer_state, 274);

    struct wl_region *input_region = wl_compositor_create_region(wl_compositor);

    // update selected icon state
    if (desktop->selected_handle && desktop->selected_handle->desktop_entry.visible)
    {
        if (lmb_state->double_click)
        {
            zwlr_foreign_toplevel_handle_v1_unset_minimized(
                desktop->selected_handle->toplevel_handle
            );
        }

        if (lmb_state->drag_start)
        {
            desktop->icon_drag_start_x = desktop->selected_handle->desktop_entry.x;
            desktop->icon_drag_start_y = desktop->selected_handle->desktop_entry.y;
            desktop->selected_handle->desktop_entry.recalc_position_when_minimize = false;
        }

        if (mmb_state->release && !mmb_state->drag_stop)
        {
            zwlr_foreign_toplevel_handle_v1_close(
                desktop->selected_handle->toplevel_handle
            );
        }

        if (lmb_state->dragging)
        {
            // redraw only when mouse moved to prevent event looping
            if ((pointer_state->x_int != pointer_state->x_old_int)
                || (pointer_state->y_int != pointer_state->y_old_int))
            {
                // clear its old position
                Desktop_AddDamage(
                    desktop,
                    desktop->selected_handle->desktop_entry.x,
                    desktop->selected_handle->desktop_entry.y,
                    DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X,
                    DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y
                );

            #if DEBUG_DAMAGE == 1
                printf("clean request '%s' on (%d, %d)\n", desktop->selected_handle->title, desktop->selected_handle->arrangement.x, desktop->selected_handle->arrangement.y);
            #endif

                int drag_dist_x = pointer_state->x_int - lmb_state->drag_start_x_int;
                int drag_dist_y = pointer_state->y_int - lmb_state->drag_start_y_int;

                // set new position and request redraw
                desktop->selected_handle->desktop_entry.x = desktop->icon_drag_start_x + drag_dist_x;
                desktop->selected_handle->desktop_entry.y = desktop->icon_drag_start_y + drag_dist_y;

                // limit position
                if (desktop->selected_handle->desktop_entry.x < -DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X / 2)
                    desktop->selected_handle->desktop_entry.x = -DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X / 2;
                if (desktop->selected_handle->desktop_entry.y < -DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y / 2)
                    desktop->selected_handle->desktop_entry.y = -DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y / 2;
                if (desktop->selected_handle->desktop_entry.x >= desktop->desktop_layer_surface.w - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X / 2))
                    desktop->selected_handle->desktop_entry.x = desktop->desktop_layer_surface.w - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X / 2) - 1;
                if (desktop->selected_handle->desktop_entry.y >= desktop->desktop_layer_surface.h - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y / 2))
                    desktop->selected_handle->desktop_entry.y = desktop->desktop_layer_surface.h - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y / 2) - 1;

                desktop->selected_handle->desktop_entry.redraw = true;
            }
        }
    }
    else
    {
        // clear some state when selected handle disappeared or invisible

        // ...?
    }

    // used to check state change
    bool pressed_on_icon = false;

    // lookup each toplevel handle
    struct HashSet_Iter iter = { };

    if (HashSet_IterInit(&iter, &desktop->toplevel_handles))
    {
        struct ToplevelHandle *toplevel_handle = NULL;

        while ((toplevel_handle = HashSet_IterNext(&iter)))
        {
            struct ToplevelHandle_DesktopEntry *arrangement =
                &toplevel_handle->desktop_entry;

            SWRenderRectI rect_this_icon =
            {
                arrangement->x,
                arrangement->y,
                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X,
                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y,
            };

            // redraw original position
            if (arrangement->redraw && arrangement->has_position)
            {
                Desktop_AddDamage(
                    desktop,
                    rect_this_icon.x,
                    rect_this_icon.y,
                    rect_this_icon.w,
                    rect_this_icon.h
                );

            #if DEBUG_DAMAGE == 1
                printf("redraw request '%s' on (%d, %d)\n", toplevel_handle->title, arrangement->x, arrangement->y);
            #endif

                arrangement->redraw = false;
            }

            // recalculate position?
            if (arrangement->visible && (arrangement->recalc_position || !arrangement->has_position))
            {
                int new_pos_x = 1;
                int new_pos_y = desktop->desktop_layer_surface.h - DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y - 1;

                rect_this_icon.x = new_pos_x;
                rect_this_icon.y = new_pos_y;

                SWRenderBoundI bounds_this_icon;
                SWRender_RectToBoundInt(&bounds_this_icon, &rect_this_icon);

                // find position
                // TODO: optimize this o(n^2) loop?
                while (true)
                {
                    bool has_overlap = false;

                    // check overlap
                    struct HashSet_Iter iter_overlap_check = { };

                    if (HashSet_IterInit(&iter_overlap_check, &desktop->toplevel_handles))
                    {
                        struct ToplevelHandle *toplevel_handle_overlap_chk = NULL;

                        while ((toplevel_handle_overlap_chk = HashSet_IterNext(&iter_overlap_check)))
                        {
                            // dont check self.
                            if (toplevel_handle_overlap_chk == toplevel_handle)
                                continue;

                            struct ToplevelHandle_DesktopEntry *arrangement_overlap_chk =
                                &toplevel_handle_overlap_chk->desktop_entry;

                            // skip invisible
                            if (!arrangement_overlap_chk->visible || !arrangement_overlap_chk->has_position)
                                continue;

                            // do overlap check
                            SWRenderBoundI bound_overlap_chk_icon;
                            SWRender_RectToBoundInt(&bound_overlap_chk_icon, &(SWRenderRectI)
                            {
                                arrangement_overlap_chk->x,
                                arrangement_overlap_chk->y,
                                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X,
                                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y,
                            });

                            if (SWRender_GetIntersectionBetweenBoundInt(&bounds_this_icon, &bound_overlap_chk_icon, NULL))
                            {
                                has_overlap = true;
                                break;
                            }
                        }
                    }

                    if (!has_overlap)
                        break;

                    // go right
                    new_pos_x += DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X + 1;
                    if (new_pos_x + (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X + 1) >= desktop->w)
                    {
                        // go left and go up
                        new_pos_x = 1;
                        new_pos_y -= DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y + 1;
                    }
                    
                    // update bounds to new position
                    rect_this_icon.x = new_pos_x;
                    rect_this_icon.y = new_pos_y;
                    SWRender_RectToBoundInt(&bounds_this_icon, &rect_this_icon);
                }

                // update position
                arrangement->x = new_pos_x;
                arrangement->y = new_pos_y;

                // and redraw at new position
                if (arrangement->visible)
                {
                    Desktop_AddDamage(desktop, arrangement->x, arrangement->y, DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X, DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y);

                    printf("redraw request '%s' on (%d, %d)\n", toplevel_handle->title ? toplevel_handle->title : "NULL", arrangement->x, arrangement->y);
                }

                arrangement->redraw = false;

                arrangement->recalc_position = false;

                arrangement->has_position = true;
            }

            if (arrangement->visible && arrangement->has_position)
            {
                wl_region_add(input_region, rect_this_icon.x, rect_this_icon.y, rect_this_icon.w, rect_this_icon.h);

                if ((lmb_state->pressed || mmb_state->pressed) && SWRender_IsVec2IInsideRectI((SWRenderVec2I){ pointer_state->x_int, pointer_state->y_int }, &rect_this_icon))
                {
                    pressed_on_icon = true;
                    desktop->selected_handle = toplevel_handle;
                }
            }
        }
    }

    // selected to nothing
    if (lmb_state->pressed && !pressed_on_icon)
        desktop->selected_handle = NULL;

#if DEBUG_DAMAGE == 1
    desktop->debug_key_pressed = false;
    if (rmb_state->pressed)
        desktop->debug_key_pressed = true;
#endif

    // add input region
    wl_surface_set_input_region(desktop->desktop_layer_surface.double_buffered_surface.surface, input_region);
    wl_region_destroy(input_region);

    PointerState_PostDispatch(pointer_state);
}

bool Desktop_AddDamage(struct Desktop *desktop, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!desktop)
        return false;

    if (w <= 0 || h <= 0)
        return false;

    SWRenderBoundI bounds;
    SWRender_RectToBoundInt(&bounds, &(SWRenderRectI){ x, y, w, h });

    return Vector_Add(&desktop->damage_rect_bounds, &bounds);
}

void Desktop_CheckRedrawAndTryCommit(struct Desktop *desktop)
{
    if (!desktop)
        return;

    if (!desktop->desktop_layer_surface.double_buffered_surface.surface)
        return;

    struct Vector_Iter iter_damage_bounds = { };

    if (desktop->damage_rect_bounds.count > 0 && Vector_IterInit(&iter_damage_bounds, &desktop->damage_rect_bounds))
    {
        desktop->damaged = true;

        SWRenderBuffer *render_buf = &desktop->desktop_layer_surface.double_buffered_surface.render_buffer;

        // for breakpoint here
    #if DEBUG_DAMAGE == 1
        if (desktop->debug_key_pressed)
            puts("debug key pressed");
    #endif

        SWRenderBoundI redraw_bounds = {
            render_buf->w,
            render_buf->h,
            -1,
            -1,
        };

        // calculate region to redraw
        SWRenderBoundI *damage_bounds = NULL;

        while ((damage_bounds = Vector_IterNext(&iter_damage_bounds)))
        {
            SWRender_IncludeBoundToBoundInt(&redraw_bounds, damage_bounds);
        #if DEBUG_DAMAGE == 1
            printf("damage bounds (%d, %d) - (%d, %d)\n", damage_bounds->x1, damage_bounds->y1, damage_bounds->x2, damage_bounds->y2);
        #endif
        }
        Vector_Clear(&desktop->damage_rect_bounds);

        // negative x1/y1 bound causes visual artifacts so limit it to buffer bounds
        SWRenderBoundI buffer_bounds;
        SWRender_BufferGetBounds(&buffer_bounds, render_buf);
        SWRender_GetIntersectionBetweenBoundInt(&redraw_bounds, &buffer_bounds, &redraw_bounds);

        // ----------------------- do redraw stuff here -----------------------

        SWRenderRectI redraw_rect;
        SWRender_BoundToRectInt(&redraw_rect, &redraw_bounds);

        SWRenderBuffer render_buf_redraw;

        if (SWRender_BufferCrop(&render_buf_redraw, render_buf, &redraw_bounds))
        {
            SWRenderBoundI redraw_local_bounds;
            SWRender_BufferGetBounds(&redraw_local_bounds, &render_buf_redraw);

            // background
            SWRender_FillRect(&render_buf_redraw, &redraw_local_bounds, (SWRenderColor){ 0, 0, 0, 0 }, true);
            
            // try redraw each icons
            struct HashSet_Iter iter_toplevel_handles = { };
            
            if (HashSet_IterInit(&iter_toplevel_handles, &desktop->toplevel_handles))
            {
                struct ToplevelHandle *toplevel_handle = NULL;
                
                while ((toplevel_handle = HashSet_IterNext(&iter_toplevel_handles)))
                {
                    if (!(toplevel_handle->desktop_entry.visible && toplevel_handle->desktop_entry.has_position))
                        continue;
                    
                    // get bounds of icon
                    SWRenderRectI icon_rect =
                    {
                        toplevel_handle->desktop_entry.x - redraw_rect.x,
                        toplevel_handle->desktop_entry.y - redraw_rect.y,
                        DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X,
                        DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y,
                    };
                    SWRenderBoundI icon_bounds;
                    SWRender_RectToBoundInt(&icon_bounds, &icon_rect);

                    // skip if not inside redraw region
                    if (!SWRender_GetIntersectionBetweenBoundInt(&redraw_local_bounds, &icon_bounds, NULL))
                        continue;

                #if DEBUG_DAMAGE == 1
                    printf("redrawing '%s' at (%d, %d)\n", toplevel_handle->title, toplevel_handle->arrangement.x, toplevel_handle->arrangement.y);
                #endif

                    // draw icon background
                    SWRender_FillRectRounded(&render_buf_redraw, &(SWRenderBoundF){ icon_bounds.x1, icon_bounds.y1, icon_bounds.x2, icon_bounds.y2 }, 5.0f, (SWRenderColor){ 255, 255, 255, 63 });

                    // draw text
                    SWRenderBuffer render_buf_icon_draw;
                    if (SWRender_BufferCrop(&render_buf_icon_draw, &render_buf_redraw, &icon_bounds))
                    {
                        // buf -> buf_redraw -> buf_icon_draw
                        int offset_x = (icon_bounds.x1 < 0) ? icon_bounds.x1 : 0;
                        int offset_y = (icon_bounds.y1 < 0) ? icon_bounds.y1 : 0;

                        bool selected = (desktop->selected_handle == toplevel_handle);

                        FTEssentials_SetSize96DPI(&global_ft_state, FLOAT_TO_F26DOT6(DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TEXT_SIZE));
                        global_ft_state.linespace_multiplier = 0.775F;

                    #if 0
                        FTRender_RenderStrWithAlign(
                            &global_ft_state,
                            &buf_icon_draw,
                            // TODO: use relative coord. instad of (0, 0)
                            // this (0, 0) coordinate introduces bug but i want to keep it a for while until finished
                            // because it somehow funny looks like some unresponsive windows in Windows GDI era
                            INT_TO_F26DOT6(buf_icon_draw.w / 2),
                            INT_TO_F26DOT6(
                                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TOP_PADDING +
                                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_Y +
                                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TEXT_SPACING
                            ),
                            toplevel_handle->title,
                            SIZE_MAX,
                            0, -1,
                            (SWRenderColor){ 0, 0, 0, 255 }
                        );
                    #endif

                        int text_top_y = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TOP_PADDING
                                    + DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_Y
                                    + DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TEXT_SPACING;

                        FTRenderWordWrap_RenderStrWithAlign(
                            &global_ft_state,
                            &global_ft_line_buffer,
                            &render_buf_icon_draw,
                        #if 0
                            // TODO: use relative offset.
                            // this introduces bug but i want to keep it a for while until finished
                            // because it somehow funny looks like some unresponsive windows in Windows GDI era
                            &(SWRenderRectF){
                                0,
                                text_top_y,
                                buf_icon_draw.w,
                                buf_icon_draw.h - text_top_y
                            },
                        #else
                            &(SWRenderRectF){
                                offset_x,
                                text_top_y + offset_y,
                                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X,
                                DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y - text_top_y
                            },
                        #endif
                            toplevel_handle->title,
                            0, -1,
                            (SWRenderColor){ 0, 0, 0, 255 }
                        );

                        SWRenderVec2I icon_draw_pos = 
                        {
                            offset_x + (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X - DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_X) / 2,
                            offset_y + DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TOP_PADDING
                        };
                        SWRender_CopyBuffer(&toplevel_handle->desktop_entry.icon_buf, NULL, &render_buf_icon_draw, icon_draw_pos);
                    }
                }
            }

            // add damage
            DoubleBufferedSurface_AddDamage(&desktop->desktop_layer_surface.double_buffered_surface, (struct Buffer_Rect *)&redraw_rect);
        }
    }

    if (desktop->damaged)
    {
        if (DoubleBufferedSurface_TryCommit(&desktop->desktop_layer_surface.double_buffered_surface))
            desktop->damaged = false;
    }
}
