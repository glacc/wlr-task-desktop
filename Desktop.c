#include "Desktop.h"

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <malloc.h>
#include <limits.h>

#include "Utility.h"

#include "ToplevelHandle.h"

#include "Globals.h"

#include "SWRender/FTEssentials.h"
#include "SWRender/FTRender.h"

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

    if (!desktop)
    {
        fprintf(stderr, "failed to allocate memory for struct Output: %s\n", strerror(errno));
        return NULL;
    }

    if (!HashSet_Init(&desktop->toplevel_handles))
    {
        fprintf(stderr, "failed to initialize struct HashSet for struct Output: %s\n", strerror(errno));
        free(desktop);
        return NULL;
    }

    // init damage rects vector
    if (!Vector_Init(&desktop->damage_bounds, sizeof(SWRenderBoundI)))
    {
        HashSet_Cleanup(&desktop->toplevel_handles);
        free(desktop);
        return NULL;
    }

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
    Vector_Cleanup(&desktop->damage_bounds);

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

    struct PointerState *pointer_state = &desktop->desktop_layer_surface.surface.pointer_state;
    struct PointerBtnState *lmb_state = Pointer_GetBtnState(pointer_state, 272);

    // update selected icon state

    //if (!lmb_state->is_down)
    //    desktop->selected_handle = NULL;

    if (desktop->selected_handle && desktop->selected_handle->arrangement.visible)
    {
        if (lmb_state->double_click)
        {
            zwlr_foreign_toplevel_handle_v1_unset_minimized(
                desktop->selected_handle->toplevel_handle
            );
        }

        if (lmb_state->drag_start)
        {
            desktop->icon_drag_start_x = desktop->selected_handle->arrangement.x;
            desktop->icon_drag_start_y = desktop->selected_handle->arrangement.y;
            desktop->selected_handle->arrangement.recalc_position_when_minimize = false;
        }

        if (lmb_state->dragging)
        {
            // redraw only when mouse moved to prevent event looping
            if ((pointer_state->x_int != pointer_state->x_old_int)
                || (pointer_state->y_int != pointer_state->y_old_int))
            {
                int drag_dist_x = pointer_state->x_int - lmb_state->drag_start_x_int;
                int drag_dist_y = pointer_state->y_int - lmb_state->drag_start_y_int;

                // clear its old position
                Desktop_AddDamage(
                    desktop,
                    desktop->selected_handle->arrangement.x,
                    desktop->selected_handle->arrangement.y,
                    DESKTOP_DEFAULT_ICON_SIZE_X,
                    DESKTOP_DEFAULT_ICON_SIZE_Y
                );

                // set new position and request redraw
                desktop->selected_handle->arrangement.x = desktop->icon_drag_start_x + drag_dist_x;
                desktop->selected_handle->arrangement.y = desktop->icon_drag_start_y + drag_dist_y;
                desktop->selected_handle->arrangement.redraw = true;
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
            struct ToplevelHandle_DesktopIconArrangement *arrangement =
                &toplevel_handle->arrangement;

            SWRenderRectI rect_this_icon =
            {
                arrangement->x,
                arrangement->y,
                DESKTOP_DEFAULT_ICON_SIZE_X,
                DESKTOP_DEFAULT_ICON_SIZE_Y,
            };

            // redraw original position
            if (arrangement->redraw && arrangement->has_position)
            {
                Desktop_AddDamage(desktop, arrangement->x, arrangement->y, DESKTOP_DEFAULT_ICON_SIZE_X, DESKTOP_DEFAULT_ICON_SIZE_Y);
                printf("redraw request '%s' on (%d, %d)\n", toplevel_handle->title, arrangement->x, arrangement->y);
                arrangement->redraw = false;
            }

            // skip if not visible
            if (!arrangement->visible)
                continue;

            // recalculate position?
            if (arrangement->recalc_position || !arrangement->has_position)
            {
                int new_pos_x = 0;
                int new_pos_y = desktop->desktop_layer_surface.h - DESKTOP_DEFAULT_ICON_SIZE_Y - 1;

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

                            struct ToplevelHandle_DesktopIconArrangement *arrangement_overlap_chk =
                                &toplevel_handle_overlap_chk->arrangement;

                            // skip invisible
                            if (!arrangement_overlap_chk->visible || !arrangement_overlap_chk->has_position)
                                continue;

                            // do overlap check
                            SWRenderBoundI bound_overlap_chk_icon;
                            SWRender_RectToBoundInt(&bound_overlap_chk_icon, &(SWRenderRectI)
                            {
                                arrangement_overlap_chk->x,
                                arrangement_overlap_chk->y,
                                DESKTOP_DEFAULT_ICON_SIZE_X,
                                DESKTOP_DEFAULT_ICON_SIZE_Y,
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

                    new_pos_x += DESKTOP_DEFAULT_ICON_SIZE_X;
                    if (new_pos_x + DESKTOP_DEFAULT_ICON_SIZE_X >= desktop->w)
                    {
                        new_pos_x = 0;
                        new_pos_y -= DESKTOP_DEFAULT_ICON_SIZE_Y;
                    }
                    
                    // update bounds to new position
                    rect_this_icon.x = new_pos_x;
                    rect_this_icon.y = new_pos_y;
                    SWRender_RectToBoundInt(&bounds_this_icon, &rect_this_icon);
                }

                arrangement->x = new_pos_x;
                arrangement->y = new_pos_y;

                // redraw new position
                if (arrangement->visible)
                {
                    Desktop_AddDamage(desktop, arrangement->x, arrangement->y, DESKTOP_DEFAULT_ICON_SIZE_X, DESKTOP_DEFAULT_ICON_SIZE_Y);

                    printf("redraw request '%s' on (%d, %d)\n", toplevel_handle->title ? toplevel_handle->title : "NULL", arrangement->x, arrangement->y);
                }

                arrangement->redraw = false;

                arrangement->recalc_position = false;

                arrangement->has_position = true;
            }

            if (lmb_state->pressed && SWRender_IsVec2IInsideRectI((SWRenderVec2I){ pointer_state->x_int, pointer_state->y_int }, &rect_this_icon))
            {
                pressed_on_icon = true;
                desktop->selected_handle = toplevel_handle;
            }
        }
    }

    if (lmb_state->pressed && !pressed_on_icon)
        desktop->selected_handle = NULL;

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

    return Vector_Add(&desktop->damage_bounds, &bounds);
}

void Desktop_CheckRedrawAndTryCommit(struct Desktop *desktop)
{
    if (!desktop)
        return;

    if (!desktop->desktop_layer_surface.surface.surface)
        return;

    struct Vector_Iter iter_damage_bounds = { };

    if (desktop->damage_bounds.count > 0 && Vector_IterInit(&iter_damage_bounds, &desktop->damage_bounds))
    {
        desktop->damaged = true;

        int swr_buf_w = desktop->desktop_layer_surface.surface.swr_buf.w;
        int swr_buf_h = desktop->desktop_layer_surface.surface.swr_buf.h;

        SWRenderBoundI redraw_region_bounds = {
            swr_buf_w,
            swr_buf_h,
            -1,
            -1,
        };

        // calculate region to redraw
        SWRenderBoundI *damage_bound = NULL;

        while ((damage_bound = Vector_IterNext(&iter_damage_bounds)))
            SWRender_IncludeBoundToBoundInt(&redraw_region_bounds, damage_bound);
        Vector_Clear(&desktop->damage_bounds);

        // TODO: do more valid check here?

        // ----------------------- do redraw stuff here -----------------------

        SWRenderRectI redraw_region_rect;
        SWRender_BoundToRectInt(&redraw_region_rect, &redraw_region_bounds);

        SWRenderBuffer buf_redraw;

        if (SWRender_BufferCrop(&buf_redraw, &desktop->desktop_layer_surface.surface.swr_buf, &redraw_region_bounds))
        {
            SWRenderBoundI redraw_local_bound;
            SWRender_BufferGetBounds(&redraw_local_bound, &buf_redraw);

            // background
            SWRender_FillRect(&buf_redraw, &redraw_local_bound, (SWRenderColor){ 0, 0, 0, 0 }, true);
            
            // icons
            struct HashSet_Iter iter_toplevel_handles = { };
            
            if (HashSet_IterInit(&iter_toplevel_handles, &desktop->toplevel_handles))
            {
                struct ToplevelHandle *toplevel_handle = NULL;
                
                while ((toplevel_handle = HashSet_IterNext(&iter_toplevel_handles)))
                {
                    if (!(toplevel_handle->arrangement.visible && toplevel_handle->arrangement.has_position))
                        continue;

                    printf("redrawing '%s' at (%d, %d)\n", toplevel_handle->title, toplevel_handle->arrangement.x, toplevel_handle->arrangement.y);
                    
                    SWRenderRectI icon_rect =
                    {
                        toplevel_handle->arrangement.x - redraw_region_rect.x + 1,
                        toplevel_handle->arrangement.y - redraw_region_rect.y + 1,
                        DESKTOP_DEFAULT_ICON_SIZE_X - 2,
                        DESKTOP_DEFAULT_ICON_SIZE_Y - 2,
                    };
                    SWRenderBoundI icon_bounds;
                    SWRender_RectToBoundInt(&icon_bounds, &icon_rect);

                    // not inside region, skip
                    if (!SWRender_GetIntersectionBetweenBoundInt(&redraw_local_bound, &icon_bounds, NULL))
                        continue;

                    // draw
                    SWRender_FillRect(&buf_redraw, &icon_bounds, (SWRenderColor){ 255, 255, 255, 255 }, true);

                    SWRenderBuffer buf_text_draw;
                    if (SWRender_BufferCrop(&buf_text_draw, &buf_redraw, &icon_bounds))
                    {
                        FTEssentials_SetSize96DPI(&global_ft_state, FLOAT_TO_F26DOT6(9.5F));
                        FTRender_RenderStrWithAlign(
                            &global_ft_state,
                            &buf_text_draw,
                            // TODO: use relative coord. instad of (0, 0)
                            // this (0, 0) coordinate introduces bug but i want to keep it a for while until finished
                            // because it somehow funny looks like some unresponsive windows in Windows GDI era
                            INT_TO_F26DOT6(0),
                            INT_TO_F26DOT6(0),
                            toplevel_handle->title,
                            -1, -1,
                            (SWRenderColor){ 0, 0, 0, 255 }
                        );
                    }
                }
            }

            //// debug for showing redrawn regions
            //SWRender_FillRect(&desktop->desktop_layer_surface.surface.swr_buf, &redraw_region_bounds, (SWRenderColor){ 0, 0, 255, 64 }, false);

            // add damage
            DoubleBufferedSurface_AddDamage(&desktop->desktop_layer_surface.surface, (struct Buffer_Rect *)&redraw_region_rect);
        }
    }

    if (desktop->damaged)
    {
        if (DoubleBufferedSurface_TryCommit(&desktop->desktop_layer_surface.surface))
            desktop->damaged = false;
    }
}
