#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-client-core.h>
#include <wayland-client.h>

#include "viewporter-client-protocol.h"
#include "single-pixel-buffer-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include "Wayland.h"
#include "Wayland/Buffer.h"
#include "Wayland/LayerSurface.h"

struct State
{
    struct Wayland wayland;
};

static const struct Buffer_Pixel color_transition_table[] =
{
    { 0xFF, 0xFF, 0x00, 0x00 },
    { 0xFF, 0xFF, 0xFF, 0x00 },
    { 0xFF, 0x00, 0xFF, 0x00 },
    { 0xFF, 0x00, 0xFF, 0xFF },
    { 0xFF, 0x00, 0x00, 0xFF },
    { 0xFF, 0xFF, 0x00, 0xFF },
    { 0xFF, 0xFF, 0x00, 0x00 },
};
const int color_transition_table_length = (sizeof(color_transition_table) / sizeof(color_transition_table[0]));

static struct Buffer_Pixel GetTransitionColor(float alpha, float progress)
{
    float progress_total = (float)(color_transition_table_length - 1) * progress;
    float progress_index = progress_total - floorf(progress_total);

    int index = (int)progress_total;
    if (index < 0)
        index = 0;
    if (index >= color_transition_table_length - 1)
        index = color_transition_table_length - 2;

    const struct Buffer_Pixel *color_curr = &color_transition_table[index];
    const struct Buffer_Pixel *color_next = &color_transition_table[index + 1];

    float r = ((float)color_curr->r + ((float)(color_next->r - color_curr->r) * progress_index)) * alpha;
    float g = ((float)color_curr->g + ((float)(color_next->g - color_curr->g) * progress_index)) * alpha;
    float b = ((float)color_curr->b + ((float)(color_next->b - color_curr->b) * progress_index)) * alpha;
    float a = ((float)color_curr->a + ((float)(color_next->a - color_curr->a) * progress_index)) * 1.0F;

    return (struct Buffer_Pixel){ (uint8_t)a, (uint8_t)r, (uint8_t)g, (uint8_t)b };
}

static void ColorTransitionTo(struct Buffer_Pixel *pixel, struct Buffer_Pixel *color_dst, float progress)
{
    float a_old_norm = (float)pixel->a / 255.0F;
    float r_old = (float)pixel->r / a_old_norm;
    float g_old = (float)pixel->g / a_old_norm;
    float b_old = (float)pixel->b / a_old_norm;

    float a_new = (float)pixel->a + ((float)(color_dst->a - pixel->a) * progress);
    float a_new_norm = a_new / 255.0F;
    float r_new = ((float)pixel->r + ((float)(color_dst->r - pixel->r) * progress)) * a_new_norm;
    float g_new = ((float)pixel->g + ((float)(color_dst->g - pixel->g) * progress)) * a_new_norm;
    float b_new = ((float)pixel->b + ((float)(color_dst->b - pixel->b) * progress)) * a_new_norm;

    pixel->a = (uint8_t)a_new;
    pixel->r = (uint8_t)r_new;
    pixel->g = (uint8_t)g_new;
    pixel->b = (uint8_t)b_new;
}

static inline void VersionMin(uint32_t *version, uint32_t supported)
{
    if (*version > supported)
        *version = supported;
}

static void TestLayerSurface(struct LayerSurface *layer_surface, struct Wayland *wayland)
{
    layer_surface->wl_surface = wl_compositor_create_surface(wayland->compositor);
    wl_surface_add_listener(layer_surface->wl_surface, &LayerSurface_wl_surface_listener, &layer_surface);

    layer_surface->zwlr_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        wayland->layer_shell,
        layer_surface->wl_surface,
        NULL,//(*(struct Output_Info **)Vector_Index(&state->state_output.outputs, 0))->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        "desktop"
    );
    //zwlr_layer_surface_v1_set_size(layer_surface->zwlr_layer_surface, w, h);
    zwlr_layer_surface_v1_set_margin(layer_surface->zwlr_layer_surface, 0, 0, 0, 0);
    zwlr_layer_surface_v1_set_anchor(layer_surface->zwlr_layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface->zwlr_layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
    zwlr_layer_surface_v1_add_listener(layer_surface->zwlr_layer_surface, &LayerSurface_zwlr_layer_surface_listener, layer_surface);

    wl_surface_commit(layer_surface->wl_surface);
    wl_display_roundtrip(wayland->display);

    struct wp_viewport *viewport = wp_viewporter_get_viewport(wayland->viewporter, layer_surface->wl_surface);
    wp_viewport_set_destination(viewport, layer_surface->w, layer_surface->h);
    
#if 1
    float alpha_norm = 0.5F;
    struct Buffer_Pixel color_bottom = { 127, 127, 127, 255 * alpha_norm };
    color_bottom.r *= alpha_norm;
    color_bottom.g *= alpha_norm;
    color_bottom.b *= alpha_norm;

    struct Buffer buffer = { };
    Buffer_Create(&buffer, wayland, layer_surface->w, layer_surface->h);

    wl_surface_attach(layer_surface->wl_surface, buffer.buffer, 0, 0);
    wl_surface_damage(layer_surface->wl_surface, 0, 0, layer_surface->w, layer_surface->h);

    struct Buffer_Pixel *ptr_row_start = buffer.pixels;
    float progress_horz_inv = 1.0F / (float)buffer.w;
    float progress_vert_inv = 1.0F / (float)(buffer.h - 1);

    for (int y = 0; y < buffer.h; y++)
    {
        struct Buffer_Pixel *ptr_pixel = ptr_row_start;
        float vert_progress = y * progress_vert_inv;

        for (int x = 0; x < buffer.w; x++)
        {
            float horz_progress = x * progress_horz_inv;
            *ptr_pixel = GetTransitionColor(alpha_norm, horz_progress);
            ColorTransitionTo(ptr_pixel, &color_bottom, vert_progress);
            ptr_pixel++;
        }

        ptr_row_start += buffer.w;
    }
#else
    struct wl_buffer *single_pix_buf = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(wayland->single_pix_buf_manager, 0xFF << 24, 0xFF << 24, 0xFF << 24, 0xFF << 24);
    wl_surface_attach(layer_surface->wl_surface, single_pix_buf, 0, 0);
    wl_surface_damage(layer_surface->wl_surface, 0, 0, layer_surface->w, layer_surface->h);
#endif

    wl_surface_commit(layer_surface->wl_surface);
    wl_display_roundtrip(wayland->display);
}

int main(int argc, char *argv[])
{
    struct State state = { };
    int ret = 0;

    if (!Wayland_Initialize(&state.wayland))
    {
        ret = EXIT_FAILURE;
        goto End;
    }

    // test layer surface
    struct LayerSurface layer_surface = { };
    TestLayerSurface(&layer_surface, &state.wayland);

    // main loop
    while (true)
    {
        if (wl_display_dispatch(state.wayland.display) == -1)
        {
            fprintf(stderr, "error while dispatching from display: %s\n", strerror(errno));
            break;
        }
    }

End:
    Wayland_Cleanup(&state.wayland);

    return ret;
}