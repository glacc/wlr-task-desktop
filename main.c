#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Globals.h"

#include <wayland-client.h>
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "Desktop.h"
#include "Keyboard.h"
#include "Pointer.h"
#include "Seat.h"
#include "ToplevelHandle.h"
#include "ToplevelManager.h"

#include "HashSet.h"
#include "Vector.h"

#include "SWRender/FTEssentials.h"
#include "SWRender/FTEssentialsWordWrap.h"

#include "Icon.h"

#ifndef DEBUG_TEST
#define DEBUG_TEST 0
#endif

#if DEBUG_TEST == 1
#include "Test/DesktopLayerSurfaceTest.h"
#endif

#define ENV_HOME            "HOME"
#define ENV_XDG_DATA_HOME   "XDG_DATA_HOME"
#define ENV_XDG_DATA_DIRS   "XDG_DATA_DIRS"

struct wl_display    *wl_display    = NULL;
struct wl_registry   *wl_registry   = NULL;

struct wl_seat       *wl_seat       = NULL;

struct wl_compositor *wl_compositor = NULL;
struct wl_shm        *wl_shm        = NULL;
struct wp_viewporter *wp_viewporter = NULL;

struct zwlr_layer_shell_v1 *zwlr_layer_shell = NULL;
struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager = NULL;

struct Keyboard global_keyboard = { };
struct Pointer  global_pointer  = { };

struct HashSet global_desktops                              = { };
struct Vector  global_desktop_with_surface_creation_pending = { };

struct HashSet global_toplevel_handles           = { };
struct Vector  global_toplevel_handle_to_destroy = { };

static int display_fd = -1;
static int local_event_pipefds[2] = { -1, -1 };

FTEssentials_State global_ft_state = { };
FTEssentialsWordWrap_LineBuffer global_ft_line_buffer = { };
static const char *fontpath = "sw-render/MiSans_Regular.ttf";

volatile sig_atomic_t global_exit = false;

void PrintEnv(const char *env_name)
{
    printf("env %s=%s\n", env_name, getenv(env_name));
}

#pragma region Wayland

static inline void WlInterfaceVerMin(uint32_t *version, uint32_t supported)
{
    if (*version > supported)
        *version = supported;
}

static void WaylandRegistryListenerGlobal(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
)
{   
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

        if (!HashSet_Add(&global_desktops, desktop, NULL))
        {
            Desktop_Destroy(desktop);
            fprintf(stderr, "failed to append Desktop\n");
            return;
        }

        // for surface creation
        if (!Vector_Add(&global_desktop_with_surface_creation_pending, &desktop))
        {
            Desktop_Destroy(desktop);
            fprintf(stderr, "failed to request layer surface creation: %s\n", strerror(errno));
            return;
        }
        const char dummy[1] = { };
        write(local_event_pipefds[1], dummy, 1);

        // succeed
        desktop->wl_output = wl_registry_bind(registry, name, &wl_output_interface, version_to_bind);
        wl_output_add_listener(desktop->wl_output, &Desktop_output_wl_listener, desktop);
    }
    else if (!strcmp(interface, wl_compositor_interface.name))
    {
        // wl_compositor
        WlInterfaceVerMin(&version_to_bind, 6);

        wl_compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version_to_bind);
    }
    else if (!strcmp(interface, wl_seat_interface.name))
    {
        // wl_seat
        WlInterfaceVerMin(&version_to_bind, 9);

        wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, version_to_bind);
        wl_seat_add_listener(wl_seat, &Seat_listener, NULL);
    }
    else if (!strcmp(interface, wl_shm_interface.name))
    {
        // wl_shm
        WlInterfaceVerMin(&version_to_bind, 1);

        wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, version_to_bind);
    }
    else if (!strcmp(interface, wp_viewporter_interface.name))
    {
        // wp_viewporter
        WlInterfaceVerMin(&version_to_bind, 1);

        wp_viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface, version_to_bind);
    }
    else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name))
    {
        // zwlr_layer_shell_v1
        WlInterfaceVerMin(&version_to_bind, 4);

        zwlr_layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version_to_bind);
    }
    else if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
    {
        // zwlr_foreign_toplevel_manager_v1
        WlInterfaceVerMin(&version_to_bind, 3);

        zwlr_toplevel_manager = wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, version_to_bind);
        zwlr_foreign_toplevel_manager_v1_add_listener(zwlr_toplevel_manager, &ToplevelManager_listener, NULL);
    }
}

static void WaylandRegistryListenerGlobalRemove(
    void *data,
    struct wl_registry *wl_registry,
    uint32_t name
)
{
    // check if name belongs to wl_output and remove
    struct Desktop *desktop = NULL;
    struct Desktop output_to_find = { .wl_name = name };
    if ((desktop = HashSet_Find(&global_desktops, &output_to_find)))
        Desktop_Destroy(desktop);

    printf("global remove 0x%08X\n", name);
}

const static struct wl_registry_listener wayland_registry_listener =
{
    .global = WaylandRegistryListenerGlobal,
    .global_remove = WaylandRegistryListenerGlobalRemove,
};

#pragma endregion

static bool Initialize(void)
{
    // wayland display and its fd
    if (!(wl_display = wl_display_connect(NULL)))
    {
        fprintf(stderr, "failed connecting to Wayland server: %s\n", strerror(errno));
        return false;
    }

    if ((display_fd = wl_display_get_fd(wl_display)) < 0)
    {
        fprintf(stderr, "failed to get display fd: %s\n", strerror(errno));
        return false;
    }

    // fd for app events
    if (pipe(local_event_pipefds) == -1)
    {
        fprintf(stderr, "failed to create pipe for local events: %s\n", strerror(errno));
        return false;
    }
    fcntl(
        local_event_pipefds[0],
        F_SETFL,
        fcntl(local_event_pipefds[0], F_GETFL) | O_NONBLOCK
    );

    // wayland registry
    if (!(wl_registry = wl_display_get_registry(wl_display)))
    {
        fprintf(stderr, "failed to get registry from display: %s\n", strerror(errno));
        return false;
    }

    // TODO: keyboard
    //if (!Keyboard_Init(&keyboard))
    //{ ...

    if (!Pointer_Init(&global_pointer))
    {
        fprintf(stderr, "failed to init struct Pointer: %s\n", strerror(errno));
        return false;
    }

    // hashset for desktops
    if (!HashSet_Init(&global_desktops))
    {
        fprintf(stderr, "failed to init HashSet for struct Output: %s\n", strerror(errno));
        return false;
    }
    global_desktops.Hash    = Desktop_Hash;
    global_desktops.IsEqual = Desktop_IsEqual;

    // hashset for toplevel handles
    if (!HashSet_Init(&global_toplevel_handles))
    {
        fprintf(stderr, "failed to init HashSet for struct ToplevelHandle: %s\n", strerror(errno));
        return false;
    }

    if (!Vector_Init(&global_toplevel_handle_to_destroy, sizeof(struct ToplevelHandle *)))
    {
        fprintf(stderr, "failed to init HashSet for toplevel handle cleanup: %s\n", strerror(errno));
        return false;
    }

    // vector for desktop surface creation
    if (!Vector_Init(&global_desktop_with_surface_creation_pending, sizeof(struct Desktop *)))
    {
        fprintf(stderr, "failed to init Vector for desktop surface creation: %s\n", strerror(errno));
        return false;
    }

    if (!FTEssentials_InitState(&global_ft_state, fontpath))
    {
        fprintf(stderr, "failed to init FreeType: %s\n", strerror(errno));
        return false;
    }

    if (!FTEssentialsWordWrap_LineBufferInit(&global_ft_line_buffer))
    {
        fprintf(stderr, "failed to init line buffer for FTRender: %s\n", strerror(errno));
        return false;
    }

    // bind registries
    wl_registry_add_listener(wl_registry, &wayland_registry_listener, NULL);
    wl_display_roundtrip(wl_display);

    // roundtrip for callbacks
    wl_display_roundtrip(wl_display);
    wl_display_roundtrip(wl_display);
    wl_display_roundtrip(wl_display);

    return true;
}

static void Cleanup(void)
{
    struct HashSet_Iter iter_hashset = { };

    // toplevel handles cleanup
    if (HashSet_IterInit(&iter_hashset, &global_toplevel_handles))
    {
        struct ToplevelHandle *handle = NULL;

        while ((handle = HashSet_IterDel(&iter_hashset)))
            ToplevelHandle_Destroy(handle);
    }
    HashSet_Cleanup(&global_toplevel_handles);
    Vector_Cleanup(&global_toplevel_handle_to_destroy);

    // desktops cleanup
    Vector_Cleanup(&global_desktop_with_surface_creation_pending);

    if (HashSet_IterInit(&iter_hashset, &global_desktops))
    {
        struct Desktop *desktop = NULL;
        while ((desktop = HashSet_IterDel(&iter_hashset)))
            Desktop_Destroy(desktop);
    }
    HashSet_Cleanup(&global_desktops);

    // keyboard & pointer
    //Keyboard_Cleanup(&keyboard);
    Pointer_Cleanup(&global_pointer);

    // fd for local events
    for (int i = 0; i < 2; i++)
    {
        if (local_event_pipefds[i] >= 0)
            close(local_event_pipefds[i]);
    }

    // wayland client objects
    if (zwlr_layer_shell)
        zwlr_layer_shell_v1_destroy(zwlr_layer_shell);

    if (zwlr_toplevel_manager)
        zwlr_foreign_toplevel_manager_v1_destroy(zwlr_toplevel_manager);

    if (wp_viewporter)
        wp_viewporter_destroy(wp_viewporter);

    if (wl_shm)
        wl_shm_destroy(wl_shm);

    if (wl_seat)
        wl_seat_destroy(wl_seat);

    if (wl_compositor)
        wl_compositor_destroy(wl_compositor);

    if (wl_display)
        wl_display_disconnect(wl_display);

    FTEssentialsWordWrap_LineBufferCleanup(&global_ft_line_buffer);
    FTEssentials_Done(&global_ft_state);
}

void ProcessDesktopCreation(void)
{
    struct Vector_Iter iter = { };

    wl_display_roundtrip(wl_display);

    if (Vector_IterInit(&iter, &global_desktop_with_surface_creation_pending))
    {
        struct Desktop **ptr_desktop_ptr = NULL;
        while ((ptr_desktop_ptr = (struct Desktop **)Vector_IterNext(&iter)))
        {
            struct Desktop *desktop = *ptr_desktop_ptr;

            DesktopLayerSurface_Init(&desktop->desktop_layer_surface, desktop->wl_output);
            DoubleBufferedSurface_SetName(&desktop->desktop_layer_surface.double_buffered_surface, desktop->name);

        #if DEBUG_TEST == 1
            TestPatternDrawOnDesktopLayerSurface(&desktop->desktop_layer_surface);
            TestSWRenderOnDesktopLayerSurface(&desktop->desktop_layer_surface);
        #endif

            DoubleBufferedSurface_AddDamage(&desktop->desktop_layer_surface.double_buffered_surface, NULL);
            DoubleBufferedSurface_TryCommit(&desktop->desktop_layer_surface.double_buffered_surface);

            Desktop_AddDamage(desktop, 0, 0, desktop->w, desktop->h);

            wl_display_roundtrip(wl_display);
        }
    }
    Vector_Clear(&global_desktop_with_surface_creation_pending);
}

void ProcessDesktopUpdate(void)
{
    struct HashSet_Iter iter = { };

    if (HashSet_IterInit(&iter, &global_desktops))
    {
        struct Desktop *desktop = NULL;

        while ((desktop = HashSet_IterNext(&iter)))
        {
            Desktop_Update(desktop);
            Desktop_CheckRedrawAndTryCommit(desktop);
        }
    }
}

void CheckToplevelHandlerCleanup(void)
{
    struct Vector_Iter iter = { };

    if (Vector_IterInit(&iter, &global_toplevel_handle_to_destroy))
    {
        struct ToplevelHandle **ptr_toplevel_handle_ptr = NULL;

        while ((ptr_toplevel_handle_ptr = Vector_IterNext(&iter)))
        {
            struct ToplevelHandle *toplevel_handle = *ptr_toplevel_handle_ptr;

            if (toplevel_handle)
                ToplevelHandle_Destroy(toplevel_handle);
        }
    }
    Vector_Clear(&global_toplevel_handle_to_destroy);
}

bool DispatchAllEvents(void)
{
    struct pollfd pollfds[2] =
    {
        { display_fd,             POLLIN },
        { local_event_pipefds[0], POLLIN },
    };

    wl_display_prepare_read(wl_display);

    int poll_result = poll(pollfds, sizeof(pollfds) / sizeof(pollfds[0]), -1);

    // poll exception?
    if (poll_result < 0)
    {
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

    // ---------------------------- local event fd ----------------------------
    if (pollfds[1].revents & POLLIN)
    {
        // desktop creation
        ProcessDesktopCreation();

        // clear
        char discard_buf[128];
        while (read(local_event_pipefds[0], discard_buf, sizeof(discard_buf)) > 0);
    }

    // --------------------------------- any ----------------------------------
    struct HashSet_Iter iter_hashset = { };

    // desktop update
    ProcessDesktopUpdate();

    // cleanup
    CheckToplevelHandlerCleanup();

    // ------------------------------------------------------------------------

    wl_display_flush(wl_display);

    return true;
}

static const char sigint_str[] = "SIGINT caught\n";

static void SIGINTHandler(int signal)
{
    write(STDERR_FILENO, sigint_str, sizeof(sigint_str));
    global_exit = 1;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (signal(SIGINT, SIGINTHandler) == SIG_ERR)
        fprintf(stderr, "failed to set signal handler: %s\n", strerror(errno));

    PrintEnv(ENV_HOME);
    PrintEnv(ENV_XDG_DATA_HOME);
    PrintEnv(ENV_XDG_DATA_DIRS);

    bool init_success = true;
    init_success = init_success && Icon_Init();
    init_success = init_success && Initialize();
    if (!init_success)
    {
        ret = EXIT_FAILURE;
        goto End;
    }

    while (DispatchAllEvents() && !global_exit);

    puts("stopping...");

End:
    Cleanup();
    Icon_Cleanup();

    return ret;
}
