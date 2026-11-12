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
#include <memory.h>
#include <malloc.h>
#include <limits.h>

#include "ToplevelHandle.h"

#include "Globals.h"

#include "SWRender/SWRender.h"
#include "SWRender/FTEssentials.h"
#include "SWRender/FTRenderWordWrap.h"

#include "Debug.h"

int Desktop_debug_verbosity = 1;

static const char str_null[] = "NULL";

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

    desktop->output_x = x;
    desktop->output_y = y;
    desktop->output_phy_w = physical_width;
    desktop->output_phy_h = physical_height;

    Debug_Printf("output geometry:\n\tx: %d, y: %d, w: %d, h: %d\n", x, y, physical_width, physical_height);
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

    Debug_Printf("output mode: %dx%d@%d.%dHz\n", width, height, refresh / 1000, refresh % 1000);
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

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void Desktop_OutputScale(
    void *data,
    struct wl_output *wl_output,
    int32_t factor
)
{
    struct Desktop *desktop = data;

    desktop->scale = factor;

    // ----------------------------- debug output -----------------------------
    // TODO: debug output
}

static void Desktop_OutputName(
    void *data,
    struct wl_output *wl_output,
    const char *name
)
{
    struct Desktop *desktop = data;
    
    if (desktop->output_name)
        free(desktop->output_name);

    desktop->output_name = strdup(name);

    Debug_Printf(
        "output of desktop: %s\n",
        desktop->output_name ? desktop->output_name : str_null
    );
}

static void Desktop_OutputDescription(
    void *data,
    struct wl_output *wl_output,
    const char *description
)
{
    struct Desktop *desktop = data;

    if (desktop->output_description)
        free(desktop->output_description);

    desktop->output_description = strdup(description);

    Debug_Printf(
        "output description: %s\n",
        desktop->output_description ? desktop->output_description : str_null
    );
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

#pragma region DesktopHashSetOperators

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

#pragma endregion

struct Desktop *Desktop_Create(void)
{
    struct Desktop *desktop = calloc(1, sizeof(*desktop));
    Vector_Init(&desktop->damage_bounds, sizeof(SWRenderBoundI));
    return desktop;
}

void Desktop_Destroy(struct Desktop *desktop)
{
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *toplevel_handle_node = NULL;
    struct ToplevelHandle *toplevel_handle = NULL;

    // free strings
    if (desktop->output_name)
        free(desktop->output_name);
    if (desktop->output_description)
        free(desktop->output_description);

    // unref ToplevelHandles
    LinkedList_IterInit(&iter, &desktop->toplevel_handles);
    while ((toplevel_handle_node = LinkedList_IterDel(&iter)))
    {
        toplevel_handle = toplevel_handle_node->elem;

        HashSet_Del(&toplevel_handle->is_inside_desktop, desktop);

        if (toplevel_handle->last_iconified_to == desktop)
            toplevel_handle->last_iconified_to = NULL;

        // change iconify_to_desktop if == this desktop
        // TODO: guessing main display by earilest name value?
        if (toplevel_handle->iconify_to_desktop == desktop)
        {
            struct HashSet_Iter iter_toplevel_handle_output = { };

            if (HashSet_IterInit(&iter_toplevel_handle_output, &toplevel_handle->is_inside_desktop))
                toplevel_handle->iconify_to_desktop = HashSet_IterNext(&iter_toplevel_handle_output);
            else
                toplevel_handle->iconify_to_desktop = NULL;
        }

        LinkedList_DestroyNode(toplevel_handle_node);
    }
    
    Vector_Cleanup(&desktop->damage_bounds);

    if (desktop->surface_created)
        DesktopSurface_Cleanup(&desktop->desktop_surface);

    wl_output_destroy(desktop->wl_output);

    free(desktop);
}

void Desktop_AddDamage(struct Desktop *desktop, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (w <= 0 || h <= 0)
        return;

    SWRenderBoundI bounds;
    SWRender_RectToBoundInt(&bounds, &(SWRenderRectI){ x, y, w, h });

    Vector_Add(&desktop->damage_bounds, &bounds);

    desktop->damaged = true;
}

void Desktop_AddDamageToplevelHandle(struct Desktop *desktop, struct ToplevelHandle *toplevel_handle)
{
    Desktop_AddDamage(
        desktop,
        toplevel_handle->desktop_entry.x,
        toplevel_handle->desktop_entry.y,
        toplevel_handle->desktop_entry.w,
        toplevel_handle->desktop_entry.h
    );
}

void Desktop_Update(struct Desktop *desktop)
{
    if (!desktop->desktop_surface.surface.wl_surface)
        return;

    struct PointerState *pointer_state = &desktop->desktop_surface.surface.pointer_state;
    struct PointerBtnState *lmb_state = Pointer_GetBtnState(pointer_state, 272);
    struct PointerBtnState *rmb_state = Pointer_GetBtnState(pointer_state, 273);
    struct PointerBtnState *mmb_state = Pointer_GetBtnState(pointer_state, 274);

    bool pressed_on_icon = false;

    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *toplevel_handle_node = NULL;
    struct ToplevelHandle *toplevel_handle = NULL;

    struct wl_region *input_region = wl_compositor_create_region(wl_compositor);

    // set cursor shape
    if (global_pointer.wp_cursor_shape_device && pointer_state->pointer_inside)
    {
        wp_cursor_shape_device_v1_set_shape(
            global_pointer.wp_cursor_shape_device,
            desktop->desktop_surface.surface.pointer_enter_serial,
            WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT
        );
    }

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
                Desktop_AddDamageToplevelHandle(desktop, desktop->selected_handle);

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
                if (desktop->selected_handle->desktop_entry.x >= desktop->desktop_surface.surface.w - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X / 2))
                    desktop->selected_handle->desktop_entry.x = desktop->desktop_surface.surface.w - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X / 2) - 1;
                if (desktop->selected_handle->desktop_entry.y >= desktop->desktop_surface.surface.h - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y / 2))
                    desktop->selected_handle->desktop_entry.y = desktop->desktop_surface.surface.h - (DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y / 2) - 1;

                desktop->selected_handle->desktop_entry.redraw = true;
            }
        }
    }
    else
    {
        // clear some state when selected handle disappeared or invisible

        // ...?
    }

    // lookup each toplevel handle
    // TODO: refactor this
    LinkedList_IterInit(&iter, &desktop->toplevel_handles);
    while ((toplevel_handle_node = LinkedList_IterNext(&iter)))
    {
        toplevel_handle = toplevel_handle_node->elem;

        // skip if not iconified to this desktop
        if (toplevel_handle->iconify_to_desktop != desktop)
            continue;

        struct ToplevelHandle_DesktopEntry *arrangement =
            &toplevel_handle->desktop_entry;

        SWRenderRectI icon_rect =
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
                icon_rect.x,
                icon_rect.y,
                icon_rect.w,
                icon_rect.h
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
            int new_pos_y = desktop->desktop_surface.surface.h - DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y - 1;

            icon_rect.x = new_pos_x;
            icon_rect.y = new_pos_y;

            SWRenderBoundI icon_bounds;
            SWRender_RectToBoundInt(&icon_bounds, &icon_rect);

            // find position
            // TODO: optimize this o(n^2) loop? add a fallback to prevent infinite loop if full?
            while (true)
            {
                bool has_overlap = false;

                // check overlap
                struct LinkedList_Iter iter_overlap_check = { };
                struct LinkedList_Node *toplevel_handle_overlap_check_node = NULL;
                struct ToplevelHandle *toplevel_handle_overlap_check = NULL;

                LinkedList_IterInit(&iter_overlap_check, &desktop->toplevel_handles);
                while ((toplevel_handle_overlap_check_node = LinkedList_IterNext(&iter_overlap_check)))
                {
                    toplevel_handle_overlap_check = toplevel_handle_overlap_check_node->elem;

                    // dont check self.
                    if (toplevel_handle_overlap_check == toplevel_handle)
                        continue;

                    struct ToplevelHandle_DesktopEntry *arrangement_overlap_chk =
                        &toplevel_handle_overlap_check->desktop_entry;

                    // skip invisible icon
                    if (!arrangement_overlap_chk->visible || !arrangement_overlap_chk->has_position)
                        continue;

                    // check overlap
                    SWRenderBoundI bound_overlap_chk_icon;
                    SWRender_RectToBoundInt(&bound_overlap_chk_icon, &(SWRenderRectI)
                    {
                        arrangement_overlap_chk->x,
                        arrangement_overlap_chk->y,
                        DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X,
                        DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y,
                    });
                    if (SWRender_GetIntersectionBetweenBoundInt(&icon_bounds, &bound_overlap_chk_icon, NULL))
                    {
                        has_overlap = true;
                        break;
                    }
                }

                // stop if not overlap
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
                icon_rect.x = new_pos_x;
                icon_rect.y = new_pos_y;
                SWRender_RectToBoundInt(&icon_bounds, &icon_rect);
            }

            // update position
            arrangement->x = new_pos_x;
            arrangement->y = new_pos_y;

            // and redraw at new position
            if (arrangement->visible)
                Desktop_AddDamage(desktop, arrangement->x, arrangement->y, DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X, DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y);

            arrangement->recalc_position = false;
            arrangement->has_position = true;
            arrangement->redraw = false;
        }

        // check pointer interactions if visible
        if (arrangement->visible && arrangement->has_position)
        {
            // has input region if visible
            wl_region_add(input_region, icon_rect.x, icon_rect.y, icon_rect.w, icon_rect.h);

            // check pointer interactions
            if ((lmb_state->pressed || mmb_state->pressed) && SWRender_IsVec2IInsideRectI((SWRenderVec2I){ pointer_state->x_int, pointer_state->y_int }, &icon_rect))
            {
                pressed_on_icon = true;

                desktop->selected_handle = toplevel_handle;
                desktop->toplevel_handle_to_send_front = toplevel_handle_node;

                Desktop_AddDamage(
                    desktop,
                    icon_rect.x,
                    icon_rect.y,
                    icon_rect.w,
                    icon_rect.h
                );
            }
        }
    }

    // selected to nothing
    if (lmb_state->pressed && !pressed_on_icon)
        desktop->selected_handle = NULL;

    // send selected handle to front
    if (desktop->toplevel_handle_to_send_front)
    {
        LinkedList_DetachNode(desktop->toplevel_handle_to_send_front);
        LinkedList_InsertNodeTail(&desktop->toplevel_handles, desktop->toplevel_handle_to_send_front);

        desktop->toplevel_handle_to_send_front = NULL;
    }

    // set input region
    wl_surface_set_input_region(desktop->desktop_surface.surface.wl_surface, input_region);
    wl_region_destroy(input_region);

    PointerState_PostDispatch(pointer_state);
}

void Desktop_Draw(struct Desktop *desktop)
{
    if (desktop->damage_bounds.count == 0)
        return;

    SWRenderBuffer *render_buffer = &desktop->desktop_surface.surface.render_buffer;
    SWRenderBoundI render_buffer_bounds;

    SWRenderBoundI damage_bounds_final = {
        render_buffer->w,
        render_buffer->h,
        -1,
        -1,
    };
    SWRenderRectI damage_rect_final;

    struct Vector_Iter iter_damage_bounds = { };
    SWRenderBoundI *damage_bounds = NULL;

    // calculate region to redraw
    Vector_IterInit(&iter_damage_bounds, &desktop->damage_bounds);
    while ((damage_bounds = Vector_IterNext(&iter_damage_bounds)))
    {
        SWRender_IncludeBoundToBoundInt(&damage_bounds_final, damage_bounds);
    #if DEBUG_DAMAGE == 1
        printf("damage bounds (%d, %d) - (%d, %d)\n", damage_bounds->x1, damage_bounds->y1, damage_bounds->x2, damage_bounds->y2);
    #endif
    }
    Vector_Clear(&desktop->damage_bounds);

    // negative x1/y1 bound causes visual artifacts so limit it to buffer bounds
    SWRender_BufferGetBounds(&render_buffer_bounds, render_buffer);
    if (!SWRender_GetIntersectionBetweenBoundInt(&damage_bounds_final, &render_buffer_bounds, &damage_bounds_final))
    {
        // skip if not inside
        desktop->damaged = false;
        return;
    }

    // -------------------- get buffer of region to redraw --------------------
    SWRenderBuffer local_redraw_render_buffer;
    SWRenderBoundI local_redraw_render_buffer_bounds;

    struct LinkedList_Iter iter_toplevel_handles = { };
    struct LinkedList_Node *toplevel_handle_node = NULL;
    struct ToplevelHandle *toplevel_handle = NULL;

    SWRender_BoundToRectInt(&damage_rect_final, &damage_bounds_final);
    if (!SWRender_BufferCrop(&local_redraw_render_buffer, render_buffer, &damage_bounds_final))
    {
        // skip if not inside
        desktop->damaged = false;
        return;
    }
    SWRender_BufferGetBounds(&local_redraw_render_buffer_bounds, &local_redraw_render_buffer);

    // -------------------------------- redraw --------------------------------
    // background
    SWRender_FillRect(&local_redraw_render_buffer, &local_redraw_render_buffer_bounds, (SWRenderColor){ 0, 0, 0, 0 }, true);
    
    // try redrawing each icons
    LinkedList_IterInit(&iter_toplevel_handles, &desktop->toplevel_handles);
    while ((toplevel_handle_node = LinkedList_IterNext(&iter_toplevel_handles)))
    {
        toplevel_handle = toplevel_handle_node->elem;

        // skip invisible icons
        if (!(toplevel_handle->desktop_entry.visible && toplevel_handle->desktop_entry.has_position))
            continue;
        
        // get icon bounds in local coordinates
        SWRenderBuffer local_icon_buffer;
        SWRenderRectI local_icon_rect =
        {
            toplevel_handle->desktop_entry.x - damage_rect_final.x,
            toplevel_handle->desktop_entry.y - damage_rect_final.y,
            toplevel_handle->desktop_entry.w,
            toplevel_handle->desktop_entry.h,
        };
        SWRenderBoundI local_icon_bounds;
        SWRender_RectToBoundInt(&local_icon_bounds, &local_icon_rect);

        // crop buffer for icon rendering
        // skip if not inside redraw region
        if (!SWRender_BufferCrop(&local_icon_buffer, &local_redraw_render_buffer, &local_icon_bounds))
            continue;

        int render_offset_x = (local_icon_rect.x < 0) ? local_icon_rect.x : 0;
        int render_offset_y = (local_icon_rect.y < 0) ? local_icon_rect.y : 0;

    #if DEBUG_DAMAGE == 1
        printf("redrawing '%s' at (%d, %d)\n", toplevel_handle->title, toplevel_handle->arrangement.x, toplevel_handle->arrangement.y);
    #endif

        // draw background
        SWRender_FillRectRounded(&local_redraw_render_buffer, &(SWRenderBoundF){ local_icon_bounds.x1, local_icon_bounds.y1, local_icon_bounds.x2, local_icon_bounds.y2 }, 5.0f, (SWRenderColor){ 255, 255, 255, 63 });

        // draw icon
        SWRenderVec2I icon_draw_pos = 
        {
            render_offset_x + (toplevel_handle->desktop_entry.w - DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_X) / 2,
            render_offset_y + DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TOP_PADDING
        };
        SWRender_CopyBuffer(&toplevel_handle->desktop_entry.icon_buf, NULL, &local_icon_buffer, icon_draw_pos);

        int text_offset_y = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TOP_PADDING
            + DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_Y
            + DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TEXT_SPACING;

        FTEssentials_SetSize96DPI(&global_ft_state, FLOAT_TO_F26DOT6(DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TEXT_SIZE));
        global_ft_state.linespace_multiplier = 0.775F;

        FTRenderWordWrap_RenderStrWithAlign(
            &global_ft_state,
            &global_ft_line_buffer,
            &local_icon_buffer,
            &(SWRenderRectF){
                render_offset_x,
                render_offset_y + text_offset_y,
                local_icon_rect.w,
                local_icon_rect.h - text_offset_y
            },
            toplevel_handle->title,
            0, -1,
            (SWRenderColor){ 0, 0, 0, 255 }
        );
    }

    // add damage
    DoubleBufferedSurface_AddDamage(&desktop->desktop_surface.surface, (struct Buffer_Rect *)&damage_rect_final);
}

bool Desktop_TryCommit(struct Desktop *desktop)
{
    if (desktop->damaged)
    {
        if (DoubleBufferedSurface_TryCommit(&desktop->desktop_surface.surface))
            desktop->damaged = false;
    }

    return !desktop->damaged;
}
