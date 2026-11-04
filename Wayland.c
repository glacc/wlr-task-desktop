#include "Wayland.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

#include "Wayland/Output.h"
#include "Wayland/Seat.h"

static inline void Wayland_VersionMin(uint32_t *version, uint32_t supported)
{
    if (*version > supported)
        *version = supported;
}

static void Wayland_RegistryListenerGlobal(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
)
{
    struct Wayland *wayland = data;
    
    uint32_t version_to_bind = version;

    printf("global 0x%08X interface: %-60s version: %u\n", name, interface, version);

    if (!strcmp(interface, wl_output_interface.name))
    {
        // wl_output
        Wayland_VersionMin(&version_to_bind, 4);

        // new object
        struct Output *output = Output_New();

        if (!output)
        {
            fprintf(stderr, "failed to allocate memory for Wayland_Output\n");
            return;
        }

        // for finding
        output->wl_name = name;

        if (!HashSet_Add(&wayland->outputs, output, NULL))
        {
            free(output);
            fprintf(stderr, "failed to append Wayland_Output\n");
            return;
        }

        // succeed
        output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, version_to_bind);
        wl_output_add_listener(output->wl_output, &Output_wl_listener, output);
    }
    else if (!strcmp(interface, wl_compositor_interface.name))
    {
        // wl_compositor
        Wayland_VersionMin(&version_to_bind, 6);

        wayland->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version_to_bind);
    }
    else if (!strcmp(interface, wl_seat_interface.name))
    {
        // wl_seat
        Wayland_VersionMin(&version_to_bind, 9);

        wayland->seat = wl_registry_bind(registry, name, &wl_seat_interface, version_to_bind);
        wl_seat_add_listener(wayland->seat, &Seat_wl_listener, wayland);
    }
    else if (!strcmp(interface, wl_shm_interface.name))
    {
        // wl_shm
        Wayland_VersionMin(&version_to_bind, 1);

        wayland->shm = wl_registry_bind(registry, name, &wl_shm_interface, version_to_bind);
    }
    else if (!strcmp(interface, wp_viewporter_interface.name))
    {
        // wp_viewporter
        Wayland_VersionMin(&version_to_bind, 1);

        wayland->viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface, version_to_bind);
    }
    else if (!strcmp(interface, wp_single_pixel_buffer_manager_v1_interface.name))
    {
        // wp_single_pixel_buffer_manager_v1
        Wayland_VersionMin(&version_to_bind, 1);

        wayland->single_pix_buf_manager = wl_registry_bind(registry, name, &wp_single_pixel_buffer_manager_v1_interface, version_to_bind);
    }
    else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name))
    {
        // zwlr_layer_shell_v1
        Wayland_VersionMin(&version_to_bind, 4);

        wayland->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version_to_bind);
    }
    else if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
    {
        // zwlr_foreign_toplevel_manager_v1
        Wayland_VersionMin(&version_to_bind, 3);

        if (!ToplevelManager_Init(&wayland->toplevel_manager))
            return;

        wayland->toplevel_manager.toplevel_manager = wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, version_to_bind);
        zwlr_foreign_toplevel_manager_v1_add_listener(wayland->toplevel_manager.toplevel_manager, &ToplevelManager_wl_listener, &wayland->toplevel_manager);
    }
}

static void RegistryListenerGlobalRemove(
    void *data,
    struct wl_registry *wl_registry,
    uint32_t name
)
{
    struct Wayland *wayland = data;

    // check if name belongs to wl_output and remove
    struct Output output_to_find = { .wl_name = name };
    struct Output *output = HashSet_Find(&wayland->outputs, &output_to_find);
    if (output)
    {
        // remove from output list
        HashSet_Del(&wayland->outputs, output);

        // destroy wayland output object
        if (output->wl_output)
            wl_output_destroy(output->wl_output);

        free(output);

        return;
    }

    printf("global remove 0x%08X\n", name);
}

const static struct wl_registry_listener registry_listener =
{
    .global = Wayland_RegistryListenerGlobal,
    .global_remove = RegistryListenerGlobalRemove,
};

bool Wayland_Initialize(struct Wayland *wayland)
{
    if (!wayland)
        return false;

    if (!(wayland->display = wl_display_connect(NULL)))
    {
        fprintf(stderr, "failed connecting to Wayland server: %s\n", strerror(errno));
        return false;
    }

    if (!(wayland->registry = wl_display_get_registry(wayland->display)))
    {
        perror("failed to get registry from display.");
        return false;
    }

    if ((wayland->display_pollfd.fd = wl_display_get_fd(wayland->display)) < 0)
    {
        perror("failed to get display fd.");
        return false;
    }
    // fcntl(wayland->fd_display, F_SETFL, fcntl(wayland->fd_display, F_GETFL) | O_NONBLOCK);
    wayland->display_pollfd.events = POLLIN;

    if (!HashSet_Init(&wayland->outputs))
    {
        perror("failed to init output hashset.");
        return false;
    }
    wayland->outputs.Hash = Output_Hash;
    wayland->outputs.IsEqual = Output_IsEqual;

    wl_registry_add_listener(wayland->registry, &registry_listener, wayland);
    wl_display_roundtrip(wayland->display);
    wl_display_roundtrip(wayland->display);
    wl_display_roundtrip(wayland->display);
    wl_display_roundtrip(wayland->display);

    return true;
}

void Wayland_Cleanup(struct Wayland *wayland)
{
    if (!wayland)
        return;

    struct HashSet_Iter hashset_iter = { };

    // zwlr objects
    if (wayland->layer_shell)
    {
        zwlr_layer_shell_v1_destroy(wayland->layer_shell);
        wayland->layer_shell = NULL;
    }

    ToplevelManager_Cleanup(&wayland->toplevel_manager);

    // wp objects
    if (wayland->viewporter)
    {
        wp_viewporter_destroy(wayland->viewporter);
        wayland->viewporter = NULL;
    }

    if (wayland->single_pix_buf_manager)
    {
        wp_single_pixel_buffer_manager_v1_destroy(wayland->single_pix_buf_manager);
        wayland->single_pix_buf_manager = NULL;
    }

    // wl objects
    Keyboard_DestroyWlObj(&wayland->keyboard);
    Pointer_DestroyWlObj(&wayland->pointer);

    if (HashSet_IterInit(&hashset_iter, &wayland->outputs))
    {
        struct Output *output = NULL;
        while ((output = HashSet_IterDel(&hashset_iter)))
        {
            if (output->wl_output)
                wl_output_destroy(output->wl_output);

            free(output);
        }
    }
    HashSet_Cleanup(&wayland->outputs);

    if (wayland->seat)
    {
        wl_seat_release(wayland->seat);
        wayland->seat = NULL;
    }

    if (wayland->compositor)
    {
        wl_compositor_release(wayland->compositor);
        wayland->compositor = NULL;
    }

    if (wayland->display)
    {
        wl_display_disconnect(wayland->display);
        wayland->display    = NULL;
        wayland->registry   = NULL;
        wayland->display_pollfd.fd = -1;
    }
}
