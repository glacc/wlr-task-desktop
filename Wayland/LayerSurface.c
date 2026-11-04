#include "LayerSurface.h"

#include <stdio.h>

static void LayerSurface_Surface_Enter(
    void *data,
    struct wl_surface *wl_surface,
    struct wl_output *output
)
{
    puts("surface entered output");
}

static void LayerSurface_Surface_Leave(
    void *data,
    struct wl_surface *wl_surface,
    struct wl_output *output
)
{
    puts("surface leaved output");
}

static void LayerSurface_Surface_PreferredBufferScale
(
    void *data,
    struct wl_surface *wl_surface,
    int32_t factor
)
{
    printf("surface preferred buf scale: %d\n", factor);
}

static void LayerSurface_Surface_PreferredBufferTransform
(
    void *data,
    struct wl_surface *wl_surface,
    uint32_t transform
)
{
    printf("surface preferred buf transform: %u\n", transform);
}

static void LayerSurface_Configure(
    void *data,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1,
    uint32_t serial,
    uint32_t width,
    uint32_t height
)
{
    struct LayerSurface *layer_surface = data;

    zwlr_layer_surface_v1_set_size(zwlr_layer_surface_v1, width, height);
    zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);

    layer_surface->w = width;
    layer_surface->h = height;

    printf(
        "layer surface configure:\n"
        "\tserial: %u\n"
        "\t%ux%u\n",
        serial, width, height
    );
}

static void LayerSurface_Closed(
    void *data,
    struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1
)
{

}

const struct wl_surface_listener LayerSurface_wl_surface_listener =
{
    .enter = LayerSurface_Surface_Enter,
    .leave = LayerSurface_Surface_Leave,
    .preferred_buffer_scale = LayerSurface_Surface_PreferredBufferScale,
    .preferred_buffer_transform = LayerSurface_Surface_PreferredBufferTransform,
};

const struct zwlr_layer_surface_v1_listener LayerSurface_zwlr_layer_surface_listener =
{
    .configure = LayerSurface_Configure,
    .closed = LayerSurface_Closed,
};
