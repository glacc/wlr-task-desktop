#include "Desktop.h"

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <malloc.h>
#include <limits.h>

#include "Utility.h"

#include "ToplevelHandle.h"

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

    DesktopLayerSurface_Cleanup(&desktop->surface);

    // destory wl_output object
    if (desktop->wl_output)
        wl_output_destroy(desktop->wl_output);

    free(desktop);
}

void Desktop_Update(struct Desktop *desktop)
{
    if (!desktop)
        return;

    struct HashSet_Iter iter = { };

    if (HashSet_IterInit(&iter, &desktop->toplevel_handles))
    {
        struct ToplevelHandle *toplevel_handle = NULL;

        while ((toplevel_handle = HashSet_IterNext(&iter)))
        {
            struct ToplevelHandle_DesktopIconArrangement *arrangement =
                &toplevel_handle->arrangement;

            //SWRenderBoundI bound_this_icon = SWRender_RectToBoundI((SWRenderRectI)
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
                arrangement->redraw = false;
            }

            if (!arrangement->visible)
                continue;

            if (arrangement->recalc_position || !arrangement->has_position)
            {
                int new_pos_x = 0;
                int new_pos_y = desktop->surface.h - DESKTOP_DEFAULT_ICON_SIZE_Y - 1;

                rect_this_icon.x = new_pos_x;
                rect_this_icon.y = new_pos_y;

                SWRenderBoundI bound_this_icon = SWRender_RectToBoundI(rect_this_icon);

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

                            if (!arrangement_overlap_chk->visible || !arrangement_overlap_chk->has_position)
                                continue;

                            SWRenderBoundI bound_overlap_chk_icon = SWRender_RectToBoundI((SWRenderRectI)
                            {
                                arrangement_overlap_chk->x,
                                arrangement_overlap_chk->y,
                                DESKTOP_DEFAULT_ICON_SIZE_X,
                                DESKTOP_DEFAULT_ICON_SIZE_Y,
                            });

                            if (SWRender_GetIntersectionBetweenBoundI(&bound_this_icon, &bound_overlap_chk_icon, NULL))
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
                    bound_this_icon = SWRender_RectToBoundI(rect_this_icon);
                }

                arrangement->x = new_pos_x;
                arrangement->y = new_pos_y;

                // redraw new position
                if (arrangement->visible)
                    Desktop_AddDamage(desktop, arrangement->x, arrangement->y, DESKTOP_DEFAULT_ICON_SIZE_X, DESKTOP_DEFAULT_ICON_SIZE_Y);

                arrangement->redraw = false;

                arrangement->recalc_position = false;

                arrangement->has_position = true;
            }
        }
    }
}

bool Desktop_AddDamage(struct Desktop *desktop, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!desktop)
        return false;

    if (w <= 0 || h <= 0)
        return false;

    SWRenderBoundI bounds = SWRender_RectToBoundI((SWRenderRectI){ x, y, w, h });

    return Vector_Add(&desktop->damage_bounds, &bounds);
}

void Desktop_CheckRedrawAndTryCommit(struct Desktop *desktop)
{
    if (!desktop)
        return;

    if (!desktop->surface.surface.surface)
        return;

    struct Vector_Iter iter_damage_bounds = { };

    if (desktop->damage_bounds.count > 0 && Vector_IterInit(&iter_damage_bounds, &desktop->damage_bounds))
    {
        desktop->damaged = true;

        int swr_buf_w = desktop->surface.surface.swr_buf.w;
        int swr_buf_h = desktop->surface.surface.swr_buf.h;

        SWRenderBoundI region_to_redraw = {
            swr_buf_w,
            swr_buf_h,
            -1,
            -1,
        };
        SWRenderBoundI *damage_bound = NULL;

        // calculate region to redraw
        while ((damage_bound = Vector_IterNext(&iter_damage_bounds)))
        {
            if (damage_bound->x1 < region_to_redraw.x1)
                region_to_redraw.x1 = damage_bound->x1;
            if (damage_bound->y1 < region_to_redraw.y1)
                region_to_redraw.y1 = damage_bound->y1;
            if (damage_bound->x2 > region_to_redraw.x2)
                region_to_redraw.x2 = damage_bound->x2;
            if (damage_bound->y2 > region_to_redraw.y2)
                region_to_redraw.y2 = damage_bound->y2;
        }
        Vector_Clear(&desktop->damage_bounds);

        if (region_to_redraw.x1 < 0)
            region_to_redraw.x1 = 0;
        if (region_to_redraw.y1 < 0)
            region_to_redraw.y1 = 0;
        if (region_to_redraw.x2 >= swr_buf_w)
            region_to_redraw.x2 = swr_buf_w - 1;
        if (region_to_redraw.y2 >= swr_buf_h)
            region_to_redraw.y2 = swr_buf_h - 1;

        // TODO: do more valid check here?

        // ----------------------- do redraw stuff here -----------------------
        int redraw_region_x = region_to_redraw.x1;
        int redraw_region_y = region_to_redraw.y1;
        int redraw_region_w = region_to_redraw.x2 - region_to_redraw.x1 + 1;
        int redraw_region_h = region_to_redraw.y2 - region_to_redraw.y1 + 1;

        SWRenderBuffer buf_redraw =
        {
            redraw_region_w,
            redraw_region_h,
            (uint32_t *)SWRender_GetPointerByPosition(
                &desktop->surface.surface.swr_buf,
                region_to_redraw.x1,
                region_to_redraw.y1
            ),
            desktop->surface.surface.swr_buf.linesize
        };
        SWRenderBoundI redraw_local_bound = SWRender_RectToBoundI((SWRenderRectI)
        {
            0,
            0,
            redraw_region_w - 1,
            redraw_region_h - 1,
        });

        struct HashSet_Iter iter_toplevel_handles;

        // background
        SWRender_FillRect(&buf_redraw, redraw_local_bound, (SWRenderColor){ 0, 0, 0, 0 }, true);
        
        // icons
        if (HashSet_IterInit(&iter_toplevel_handles, &desktop->toplevel_handles))
        {
            struct ToplevelHandle *toplevel_handle = NULL;
            
            while ((toplevel_handle = HashSet_IterNext(&iter_toplevel_handles)))
            {
                if (!(toplevel_handle->arrangement.visible && toplevel_handle->arrangement.has_position))
                    continue;
                
                SWRenderBoundI icon_bounds = SWRender_RectToBoundI((SWRenderRectI)
                {
                    toplevel_handle->arrangement.x - redraw_region_x + 1,
                    toplevel_handle->arrangement.y - redraw_region_y + 1,
                    DESKTOP_DEFAULT_ICON_SIZE_X - 2,
                    DESKTOP_DEFAULT_ICON_SIZE_Y - 2,
                    //redraw_region_x + toplevel_handle->arrangement.w - 1,
                    //redraw_region_y + toplevel_handle->arrangement.h - 1,
                });

                if (!SWRender_GetIntersectionBetweenBoundI(&redraw_local_bound, &icon_bounds, NULL))
                    continue;

                SWRender_FillRect(&buf_redraw, icon_bounds, (SWRenderColor){ 255, 255, 255, 255 }, true);
            }
        }

        // debug
        SWRender_FillRect(&desktop->surface.surface.swr_buf, region_to_redraw, (SWRenderColor){ 0, 0, 255, 64 }, false);

        // add damage
        DoubleBufferedSurface_AddDamage(&desktop->surface.surface, &(struct Buffer_Rect){
            redraw_region_x, redraw_region_y, redraw_region_w, redraw_region_h
        });
    }

    if (desktop->damaged)
    {
        if (DoubleBufferedSurface_TryCommit(&desktop->surface.surface))
            desktop->damaged = false;
    }
}
