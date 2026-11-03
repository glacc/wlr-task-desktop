#pragma once

#include <stdbool.h>

#include <sys/poll.h>

#include <wayland-client.h>

#include "viewporter-client-protocol.h"
#include "single-pixel-buffer-v1-client-protocol.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "Vector.h"
#include "Wayland/Keyboard.h"
#include "Wayland/Pointer.h"
#include "Wayland/ToplevelManager.h"

struct Wayland
{
    // wl objects
    struct wl_display    *display;
    struct pollfd        display_pollfd;

    struct wl_registry   *registry;
    struct wl_compositor *compositor;
    struct wl_seat       *seat;

    struct wl_shm        *shm;

    struct Vector   outputs;    // typeof(struct Wayland_Output *)
    struct Keyboard keyboard;
    struct Pointer  pointer;

    // wp objects
    struct wp_viewporter *viewporter;
    struct wp_single_pixel_buffer_manager_v1 *single_pix_buf_manager;

    // zwlr objects
    struct zwlr_layer_shell_v1 *layer_shell;
    struct ToplevelManager toplevel_manager;
};

extern bool Wayland_Initialize(struct Wayland *wayland);
extern void Wayland_Cleanup(struct Wayland *wayland);
