/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include <unistd.h>
#include <poll.h>

#include "seat.h"
#include "desktop.h"
#include "icon.h"

static const char fontpath[] = "./sw-render/MiSans_Regular.ttf";

struct wl_display    *wl_display  = NULL;
struct wl_registry   *wl_registry = NULL;

struct wl_compositor *wl_compositor = NULL;
struct wl_shm        *wl_shm        = NULL;

struct wp_cursor_shape_manager_v1 *wp_cursor_shape_manager = NULL;

struct zwlr_layer_shell_v1 *zwlr_layer_shell = NULL;

struct xkb_context *xkb_context = NULL;

FTEssentials_State ft_state = { };

static sig_atomic_t interrupted = 0;

#pragma region registry_listeners

static void registry_global(
    void *data,
    struct wl_registry *wl_registry,
    uint32_t name,
    const char *interface,
    uint32_t version
)
{
    if (!strcmp(interface, wl_output_interface.name))
    {
        if (version > 4)
            version = 4;

        struct wl_output *wl_output = wl_registry_bind(wl_registry, name, &wl_output_interface, version);
        desktops_add_by_wl_output(wl_output, name);
    }
    else if (!strcmp(interface, wl_seat_interface.name))
    {
        if (version > 9)
            version = 9;

        struct wl_seat *wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, version);
        seats_add(wl_seat, name);
    }
    else if (!strcmp(interface, wl_compositor_interface.name))
    {
        if (version > 6)
            version = 6;

        wl_compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
    }
    else if (!strcmp(interface, wl_shm_interface.name))
    {
        if (version > 2)
            version = 2;

        wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
    }
    else if (!strcmp(interface, wp_cursor_shape_manager_v1_interface.name))
    {
        if (version > 1)
            version = 1;

        wp_cursor_shape_manager = wl_registry_bind(wl_registry, name, &wp_cursor_shape_manager_v1_interface, version);
    }
    else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name))
    {
        if (version > 4)
            version = 4;

        zwlr_layer_shell = wl_registry_bind(wl_registry, name, &zwlr_layer_shell_v1_interface, version);
    }
    else if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
    {
        if (version > 3)
            version = 3;

        struct zwlr_foreign_toplevel_manager_v1 *zwlr_foreign_toplevel_manager = wl_registry_bind(wl_registry, name, &zwlr_foreign_toplevel_manager_v1_interface, version);
        foreign_toplevel_manager_init(zwlr_foreign_toplevel_manager);
    }
}

static void registry_global_remove(
    void *data,
    struct wl_registry *wl_registry,
    uint32_t name
)
{
    seats_pend_remove_by_name(name);
    desktops_pend_remove_by_name(name);
}

static const struct wl_registry_listener registry_listener =
{
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

#pragma endregion

static void wayland_object_cleanup(void)
{
    if (zwlr_layer_shell)
        zwlr_layer_shell_v1_destroy(zwlr_layer_shell);

    if (wl_compositor)
        wl_compositor_destroy(wl_compositor);

    if (wl_shm)
        wl_shm_destroy(wl_shm);
}

static void signal_sigint_handle(int signal)
{
    const char str_sigint_caught[] = "\nSIGINT caught...\n";
    write(STDOUT_FILENO, str_sigint_caught, sizeof(str_sigint_caught));

    interrupted = 1;
}

static bool dispatch_events(void)
{
    wl_display_flush(wl_display);

    // --------------------------------- poll ---------------------------------
    wl_display_prepare_read(wl_display);

    struct pollfd pollfds[1] = { { wl_display_get_fd(wl_display), POLLIN } };
    int poll_result = poll(pollfds, sizeof(pollfds) / sizeof(pollfds[0]), -1);

    // poll exception
    if (poll_result < 0)
    {
        wl_display_cancel_read(wl_display);

        if (errno == EINTR)
            return true;
        else
        {
            fprintf(stderr, "fail while polling fds: %s\n", strerror(errno));
            return false;
        }
    }
    
    // -------------------------- wayland display fd --------------------------
    if (pollfds[0].revents & POLLIN)
    {
        if (wl_display_read_events(wl_display) == -1)
        {
            fprintf(stderr, "error while reading events display: %s\n", strerror(errno));
            return false;
        }
        if (wl_display_dispatch_pending(wl_display) == -1)
        {
            fprintf(stderr, "error to dispatch from display: %s\n", strerror(errno));
            return false;
        }
    }
    else
        wl_display_cancel_read(wl_display);

    // ------------------------------------------------------------------------
    return true;
}

int main(void)
{
    signal(SIGINT, signal_sigint_handle);

    wl_display = wl_display_connect(NULL);
    if (!wl_display)
        exit(EXIT_FAILURE);

    if (!FTEssentials_InitState(&ft_state, fontpath))
        exit(EXIT_FAILURE);

    xkb_context = xkb_context_new(0);
    assert(xkb_context != NULL);

    icon_init();
    seats_init();
    toplevel_handles_init();
    desktops_init();

    wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &registry_listener, NULL);
    wl_display_roundtrip(wl_display);

    while (!interrupted)
    {
        if (!dispatch_events())
        {
            fprintf(stderr, "wl_display_dispatch failed: %s\n", strerror(errno));
            break;
        }

        toplevel_handles_manage_icon_loading();
        desktops_manage_creation();

        desktops_manage();

        desktops_manage_post_update();
        toplevel_handles_manage_post_update();
        seats_manage_post_update();

        desktops_manage_removal();
        toplevel_handles_manage_closed();
    }

    puts("cleaning up...");
    
    desktops_cleanup();
    toplevel_handles_cleanup();
    foreign_toplevel_manager_cleanup();
    seats_cleanup();
    icon_cleanup();

    wayland_object_cleanup();

    xkb_context_unref(xkb_context);

    wl_display_disconnect(wl_display);

    return 0;
}
