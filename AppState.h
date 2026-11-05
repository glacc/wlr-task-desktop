#pragma once

#include <stdbool.h>

#include <sys/poll.h>

#include <wayland-client.h>

#include "viewporter-client-protocol.h"
#include "single-pixel-buffer-v1-client-protocol.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "Vector.h"
#include "HashSet.h"

#include "Keyboard.h"
#include "Pointer.h"

struct AppState
{
    // wl objects
    struct wl_display    *display;

    struct wl_registry   *registry;
    uint32_t latest_name;

    struct wl_compositor *compositor;
    struct wl_seat       *seat;

    struct wl_shm        *shm;

    // wp objects
    struct wp_viewporter *viewporter;
    struct wp_single_pixel_buffer_manager_v1 *single_pix_buf_manager;

    // zwlr objects
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager;

    struct Keyboard keyboard;
    struct Pointer  pointer;

    struct HashSet desktops;            // typeof(struct Desktop *)
    struct HashSet toplevel_handles;    // typeof(struct ToplevelHandle *)

    // vector for events
    struct Vector desktops_with_surface_to_create;

    // fd for app event, 0: read, 1: write
    int pipefds[2];
    // 0: display_fd, 1: app_fd
    struct pollfd poll_fds[2];
};

extern bool AppState_Initialize(struct AppState *wayland);
extern void AppState_Cleanup(struct AppState *wayland);
