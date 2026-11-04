#include "Output.h"

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <malloc.h>

#include "Utility.h"

#include "ToplevelHandle.h"

size_t Output_Hash(void *ptr)
{
    struct Output *output = ptr;
    return output->wl_name;
}

bool Output_IsEqual(void *l, void *r)
{
    struct Output *output_l = l;
    struct Output *output_r = r;
    return (output_l->wl_name == output_r->wl_name);
}

struct Output *Output_New(void)
{
    struct Output *output = calloc(1, sizeof(*output));

    if (!output)
    {
        fprintf(stderr, "failed to allocate memory for struct Output: %s\n", strerror(errno));
        return NULL;
    }

    if (!HashSet_Init(&output->toplevel_handles))
    {
        fprintf(stderr, "failed to initialize struct HashSet for struct Output: %s\n", strerror(errno));
        free(output);
        return NULL;
    }

    return output;
}

void Output_Destroy(struct Output *output)
{
    if (!output)
        return;

    // free strings
    if (output->name)
        free(output->name);
    if (output->description)
        free(output->description);

    struct HashSet_Iter iter = { };

    // unref ToplevelHandles
    if (HashSet_IterInit(&iter, &output->toplevel_handles))
    {
        struct ToplevelHandle *toplevel_handle = NULL;

        while ((toplevel_handle = HashSet_IterDel(&iter)))
            HashSet_Del(&toplevel_handle->is_inside_outputs, output);
    }
    HashSet_Cleanup(&output->toplevel_handles);

    // destory wl_output object
    if (output->wl_output)
        wl_output_destroy(output->wl_output);
}

static void Output_Geometry(
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
    struct Output *output = data;

    output->x = x;
    output->y = y;
    output->phy_w = physical_width;
    output->phy_h = physical_height;

    printf(
        "geometry %s %s: %dx%d at (%d, %d)\n",
        make, model, physical_width, physical_height, x, y
    );
}

static void Output_Mode(
    void *data,
    struct wl_output *wl_output,
    uint32_t flags,
    int32_t width,
    int32_t height,
    int32_t refresh
)
{
    struct Output *output = data;

    output->w = width;
    output->h = height;

    printf("\tmode: %dx%d@%d.%03dHz\n", width, height, refresh / 1000, refresh % 1000);
}

static void Output_Done(
    void *data,
    struct wl_output *wl_output
)
{
    struct Output *info = data;

    info->wl_output = wl_output;

    if (!info->scale)
        info->scale = 1;

    printf("\twl_name: %u\n", info->wl_name);

    printf("output done\n");
}

static void Output_Scale(
    void *data,
    struct wl_output *wl_output,
    int32_t factor
)
{
    struct Output *output = data;

    output->scale = factor;

    printf("\tscale: %d\n", factor);
}

static void Output_Name(
    void *data,
    struct wl_output *wl_output,
    const char *name
)
{
    struct Output *output = data;

    Utility_TryReplaceWithAnotherString(&output->name, name);

    printf("\tname: %s\n", name);
}

static void Output_Description(
    void *data,
    struct wl_output *wl_output,
    const char *description
)
{
    struct Output *output = data;

    Utility_TryReplaceWithAnotherString(&output->description, description);

    printf("\tdescription: %s\n", description);
}

const struct wl_output_listener Output_wl_listener =
{
    .geometry = Output_Geometry,
    .mode = Output_Mode,
    .done = Output_Done,
    .scale = Output_Scale,
    .name = Output_Name,
    .description = Output_Description,
};
