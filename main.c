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
#include "Wayland/LayerSurface.h"

struct State
{
    struct Wayland wayland;
};

static inline void VersionMin(uint32_t *version, uint32_t supported)
{
    if (*version > supported)
        *version = supported;
}

static void TestLayerSurface(struct Wayland *wayland)
{
    struct LayerSurface_Info layer_surface_info = { };
    
    layer_surface_info.wl_surface = wl_compositor_create_surface(wayland->compositor);
    wl_surface_add_listener(layer_surface_info.wl_surface, &LayerSurface_wl_surface_listener, &layer_surface_info);

    layer_surface_info.zwlr_layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        wayland->layer_shell,
        layer_surface_info.wl_surface,
        NULL,//(*(struct Output_Info **)Vector_Index(&state->state_output.outputs, 0))->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
        "desktop"
    );
    zwlr_layer_surface_v1_set_size(layer_surface_info.zwlr_layer_surface, 640, 640);
    zwlr_layer_surface_v1_set_margin(layer_surface_info.zwlr_layer_surface, 0, 0, 0, 0);
    zwlr_layer_surface_v1_set_anchor(layer_surface_info.zwlr_layer_surface, 0);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface_info.zwlr_layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
    zwlr_layer_surface_v1_add_listener(layer_surface_info.zwlr_layer_surface, &LayerSurface_zwlr_layer_surface_listener, &layer_surface_info);

    wl_surface_commit(layer_surface_info.wl_surface);
    wl_display_roundtrip(wayland->display);

    struct wp_viewport *viewport = wp_viewporter_get_viewport(wayland->viewporter, layer_surface_info.wl_surface);
    wp_viewport_set_destination(viewport, layer_surface_info.w, layer_surface_info.h);
    
    struct wl_buffer *single_pix_buf = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(wayland->single_pix_buf_manager, 0xFF << 24, 0xFF << 24, 0xFF << 24, 0xFF << 24);
    wl_surface_attach(layer_surface_info.wl_surface, single_pix_buf, 0, 0);
    wl_surface_damage(layer_surface_info.wl_surface, 0, 0, layer_surface_info.w, layer_surface_info.h);

    wl_surface_commit(layer_surface_info.wl_surface);
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
    TestLayerSurface(&state.wayland);

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