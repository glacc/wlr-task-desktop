#include "Desktop.h"

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <malloc.h>

#include "Utility.h"

#include "ToplevelHandle.h"

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

    DesktopLayerSurface_Cleanup(&desktop->surface);

    // destory wl_output object
    if (desktop->wl_output)
        wl_output_destroy(desktop->wl_output);

    free(desktop);
}

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
