#include "Output.h"

#include <stdio.h>
#include <memory.h>
#include <malloc.h>

bool Outputs_VectorInit(struct Vector *outputs)
{
    if (!outputs)
        return false;

    if (!Vector_Init(outputs, sizeof(struct WaylandOutput *)))
        return false;

    return true;
}

void Outputs_VectorCleanup(struct Vector *outputs)
{
    if (!outputs)
        return;

    if (outputs->array)
    {
        for (int i = 0; i < outputs->count; i++)
        {
            struct Wayland_Output *output = *(struct Wayland_Output **)Vector_Index(outputs, i);

            wl_output_release(output->wl_output);

            free(output);
        }
    }

    Vector_Free(outputs);
}

void Outputs_RemoveOutputByName(struct Vector *outputs, uint32_t name)
{
    if (!outputs)
        return;

    if (!outputs->array)
        return;

    int i = 0;
    while (i < outputs->count)
    {
        struct Wayland_Output *output_info = *(struct Wayland_Output **)Vector_Index(outputs, i);
        
        if (output_info->wl_name != name)
        {
            wl_output_release(output_info->wl_output);

            Vector_RemoveAt(outputs, i);

            free(output_info);

            return;
        }

        i++;
    }
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
    struct Wayland_Output *output = data;

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
    struct Wayland_Output *output = data;

    output->w = width;
    output->h = height;

    printf("\tmode: %dx%d@%d.%03dHz\n", width, height, refresh / 1000, refresh % 1000);
}

static void Output_Done(
    void *data,
    struct wl_output *wl_output
)
{
    struct Wayland_Output *info = data;

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
    struct Wayland_Output *output = data;

    output->scale = factor;

    printf("\tscale: %d\n", factor);
}

static void Output_Name(
    void *data,
    struct wl_output *wl_output,
    const char *name
)
{
    printf("\tname: %s\n", name);
}

static void Output_Description(
    void *data,
    struct wl_output *wl_output,
    const char *description
)
{
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
