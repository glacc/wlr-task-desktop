#include "AppState.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>

#include <fcntl.h>
#include <unistd.h>

#include "Desktop.h"
#include "Seat.h"
#include "ToplevelManager.h"
#include "ToplevelHandle.h"

static inline void WlInterfaceVerMin(uint32_t *version, uint32_t supported)
{
    if (*version > supported)
        *version = supported;
}

static void AppState_RegistryListenerGlobal(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
)
{
    struct AppState *app_state = data;
    
    app_state->latest_name = name;
    
    uint32_t version_to_bind = version;

    printf("global 0x%08X interface: %-60s version: %u\n", name, interface, version);

    if (!strcmp(interface, wl_output_interface.name))
    {
        // wl_output
        WlInterfaceVerMin(&version_to_bind, 4);

        // new object
        struct Desktop *desktop = Desktop_Create();

        if (!desktop)
        {
            fprintf(stderr, "failed to allocate memory for Desktop\n");
            return;
        }

        // for finding
        desktop->wl_name = name;

        if (!HashSet_Add(&app_state->desktops, desktop, NULL))
        {
            Desktop_Destroy(desktop);
            fprintf(stderr, "failed to append Desktop\n");
            return;
        }

        // for surface creation
        if (!Vector_Add(&app_state->desktops_with_surface_to_create, &desktop))
        {
            Desktop_Destroy(desktop);
            fprintf(stderr, "failed to request layer surface creation: %s\n", strerror(errno));
            return;
        }
        const char dummy[1] = { };
        write(app_state->pipefds[1], dummy, 1);

        // succeed
        desktop->wl_output = wl_registry_bind(registry, name, &wl_output_interface, version_to_bind);
        wl_output_add_listener(desktop->wl_output, &Desktop_output_wl_listener, desktop);
    }
    else if (!strcmp(interface, wl_compositor_interface.name))
    {
        // wl_compositor
        WlInterfaceVerMin(&version_to_bind, 6);

        app_state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version_to_bind);
    }
    else if (!strcmp(interface, wl_seat_interface.name))
    {
        // wl_seat
        WlInterfaceVerMin(&version_to_bind, 9);

        app_state->seat = wl_registry_bind(registry, name, &wl_seat_interface, version_to_bind);
        wl_seat_add_listener(app_state->seat, &Seat_wl_listener, app_state);
    }
    else if (!strcmp(interface, wl_shm_interface.name))
    {
        // wl_shm
        WlInterfaceVerMin(&version_to_bind, 1);

        app_state->shm = wl_registry_bind(registry, name, &wl_shm_interface, version_to_bind);
    }
    else if (!strcmp(interface, wp_viewporter_interface.name))
    {
        // wp_viewporter
        WlInterfaceVerMin(&version_to_bind, 1);

        app_state->viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface, version_to_bind);
    }
    else if (!strcmp(interface, wp_single_pixel_buffer_manager_v1_interface.name))
    {
        // wp_single_pixel_buffer_manager_v1
        WlInterfaceVerMin(&version_to_bind, 1);

        app_state->single_pix_buf_manager = wl_registry_bind(registry, name, &wp_single_pixel_buffer_manager_v1_interface, version_to_bind);
    }
    else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name))
    {
        // zwlr_layer_shell_v1
        WlInterfaceVerMin(&version_to_bind, 4);

        app_state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version_to_bind);
    }
    else if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
    {
        // zwlr_foreign_toplevel_manager_v1
        WlInterfaceVerMin(&version_to_bind, 3);

        app_state->toplevel_manager = wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, version_to_bind);
        zwlr_foreign_toplevel_manager_v1_add_listener(app_state->toplevel_manager, &ToplevelManager_wl_listener, app_state);
    }
}

static void AppState_RegistryListenerGlobalRemove(
    void *data,
    struct wl_registry *wl_registry,
    uint32_t name
)
{
    struct AppState *app_state = data;

    // check if name belongs to wl_output and remove
    struct Desktop output_to_find = { .wl_name = name };
    struct Desktop *desktop = HashSet_Find(&app_state->desktops, &output_to_find);
    if (desktop)
        Desktop_Destroy(desktop);

    printf("global remove 0x%08X\n", name);
}

const static struct wl_registry_listener wl_registry_listener =
{
    .global = AppState_RegistryListenerGlobal,
    .global_remove = AppState_RegistryListenerGlobalRemove,
};

bool AppState_Initialize(struct AppState *app_state)
{
    if (!app_state)
        return false;

    // display
    if (!(app_state->display = wl_display_connect(NULL)))
    {
        fprintf(stderr, "failed connecting to Wayland server: %s\n", strerror(errno));
        return false;
    }

    // registry
    if (!(app_state->registry = wl_display_get_registry(app_state->display)))
    {
        fprintf(stderr, "failed to get registry from display: %s\n", strerror(errno));
        goto Error;
    }

    // display fd
    if ((app_state->poll_fds[0].fd = wl_display_get_fd(app_state->display)) < 0)
    {
        fprintf(stderr, "failed to get display fd: %s\n", strerror(errno));
        goto Error;
    }
    fcntl(app_state->poll_fds[0].fd, F_SETFL, fcntl(app_state->poll_fds[0].fd, F_GETFL) | O_NONBLOCK);
    app_state->poll_fds[0].events = POLLIN;

    // fd for app events
    app_state->pipefds[0] = -1;
    app_state->pipefds[1] = -1;
    if (pipe(app_state->pipefds) == -1)
    {
        fprintf(stderr, "failed to ");
        goto Error;
    }
    fcntl(app_state->pipefds[0], F_SETFL, fcntl(app_state->pipefds[0], F_GETFL) | O_NONBLOCK);
    app_state->poll_fds[1].fd = app_state->pipefds[0];
    app_state->poll_fds[1].events = POLLIN;
    
    // hashset for desktops
    if (!HashSet_Init(&app_state->desktops))
    {
        fprintf(stderr, "failed to init HashSet for struct Output: %s\n", strerror(errno));
        goto Error;
    }
    app_state->desktops.Hash = Desktop_Hash;
    app_state->desktops.IsEqual = Desktop_IsEqual;

    // hashset for toplevel handles
    if (!HashSet_Init(&app_state->toplevel_handles))
    {
        fprintf(stderr, "failed to init HashSet for struct ToplevelHandle: %s\n", strerror(errno));
        goto Error;
    }

    // vector for desktop surface creation
    if (!Vector_Init(&app_state->desktops_with_surface_to_create, sizeof(struct Desktop *)))
    {
        fprintf(stderr, "failed to init Vector for desktop surface creation: %s\n", strerror(errno));
        goto Error;
    }

    // latest name reserved for future use...?
    app_state->latest_name = 0;

    // bind registries
    wl_registry_add_listener(app_state->registry, &wl_registry_listener, app_state);
    wl_display_roundtrip(app_state->display);

    // roundtrip for callbacks
    wl_display_roundtrip(app_state->display);
    wl_display_roundtrip(app_state->display);
    wl_display_roundtrip(app_state->display);

    return true;

Error:
    for (int i = 0; i < 2; i++)
    {
        int fd = app_state->pipefds[i];

        if (fd >= 0)
            close(fd);
    }

    HashSet_Cleanup(&app_state->desktops);
    HashSet_Cleanup(&app_state->toplevel_handles);

    wl_display_disconnect(app_state->display);

    return false;
}

void AppState_Cleanup(struct AppState *app_state)
{
    if (!app_state)
        return;

    struct HashSet_Iter hashset_iter = { };

    // zwlr objects
    if (app_state->layer_shell)
        zwlr_layer_shell_v1_destroy(app_state->layer_shell);

    if (app_state->toplevel_manager)
        zwlr_foreign_toplevel_manager_v1_destroy(app_state->toplevel_manager);

    if (HashSet_IterInit(&hashset_iter, &app_state->toplevel_handles))
    {
        struct ToplevelHandle *handle = NULL;

        while ((handle = HashSet_IterDel(&hashset_iter)))
            ToplevelHandle_Destroy(handle);
    }
    HashSet_Cleanup(&app_state->toplevel_handles);

    // wp objects
    if (app_state->viewporter)
        wp_viewporter_destroy(app_state->viewporter);

    if (app_state->single_pix_buf_manager)
        wp_single_pixel_buffer_manager_v1_destroy(app_state->single_pix_buf_manager);

    // wl objects
    Keyboard_DestroyWlObj(&app_state->keyboard);
    Pointer_DestroyWlObj(&app_state->pointer);

    if (HashSet_IterInit(&hashset_iter, &app_state->desktops))
    {
        struct Desktop *desktop = NULL;
        while ((desktop = HashSet_IterDel(&hashset_iter)))
        {
            if (desktop->wl_output)
                wl_output_destroy(desktop->wl_output);

            free(desktop);
        }
    }
    HashSet_Cleanup(&app_state->desktops);

    if (app_state->seat)
        wl_seat_release(app_state->seat);

    if (app_state->compositor)
        wl_compositor_release(app_state->compositor);

    if (app_state->display)
        wl_display_disconnect(app_state->display);
}
