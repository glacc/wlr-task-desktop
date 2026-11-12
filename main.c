#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "Icon.h"
#include "Globals.h"

#include "Seat.h"
#include "Desktop.h"
#include "ToplevelManager.h"
#include "ToplevelHandle.h"

#include "Debug.h"

struct wl_display    *wl_display    = NULL;
struct wl_registry   *wl_registry   = NULL;

struct wl_seat       *wl_seat       = NULL;
struct wp_cursor_shape_manager_v1 *wp_cursor_shape_manager = NULL;

struct wl_compositor *wl_compositor = NULL;
struct wl_shm        *wl_shm        = NULL;
struct wp_viewporter *wp_viewporter = NULL;

struct zwlr_layer_shell_v1 *zwlr_layer_shell = NULL;
struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager = NULL;

struct Keyboard global_keyboard = { };
struct Pointer  global_pointer  = { };

struct HashSet    global_desktops         = { };
struct LinkedList global_desktops_created = { };

struct LinkedList global_toplevel_handles        = { };
struct LinkedList global_toplevel_handles_closed = { };

static int display_fd = -1;
static int event_pipe_fds[2] = { -1, -1 };

FTEssentials_State global_ft_state = { };
FTWordWrapLineBuffer global_ft_line_buffer = { };
static const char *fontpath = "sw-render/MiSans_Regular.ttf";

volatile sig_atomic_t global_interrupted = 0;
static const char sigint_str[] = "\nSIGINT caught\n";

static const char str_null[] = "NULL";

static void SignalHandlerSIGINT(int signal)
{
    write(STDERR_FILENO, sigint_str, sizeof(sigint_str));
    global_interrupted = 1;
}

#pragma region RegistryGlobalListener

static void RegistryGlobal(
    void *data,
    struct wl_registry *registry,
    uint32_t name,
    const char *interface,
    uint32_t version
)
{   
    if (!strcmp(interface, wl_output_interface.name))
    {
        // wl_output
        if (version > 4)
            version = 4;

        // new object
        struct Desktop *desktop = Desktop_Create();

        // for looking up in hashset
        desktop->wl_name = name;
        HashSet_Add(&global_desktops, desktop, NULL);

        // for surface creation
        LinkedList_InsertElemTail(&global_desktops_created, desktop);

        // send event
        const char dummy[1] = { };
        write(event_pipe_fds[1], dummy, 1);

        // succeed
        desktop->wl_output = wl_registry_bind(registry, name, &wl_output_interface, version);
        wl_output_add_listener(desktop->wl_output, &Desktop_output_wl_listener, desktop);
    }
    else if (!strcmp(interface, wl_compositor_interface.name))
    {
        // wl_compositor
        if (version > 6)
            version = 6;

        wl_compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
    }
    else if (!strcmp(interface, wl_seat_interface.name))
    {
        // wl_seat
        if (version > 9)
            version = 9;

        wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
        wl_seat_add_listener(wl_seat, &Seat_listener, NULL);
    }
    else if (!strcmp(interface, wp_cursor_shape_manager_v1_interface.name))
    {
        // wp_cursor_shape_manager
        if (version > 1)
            version = 1;

        wp_cursor_shape_manager = wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, version);
    }
    else if (!strcmp(interface, wl_shm_interface.name))
    {
        // wl_shm
        if (version > 1)
            version = 1;

        wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, version);
    }
    else if (!strcmp(interface, wp_viewporter_interface.name))
    {
        // wp_viewporter
        if (version > 1)
            version = 1;

        wp_viewporter = wl_registry_bind(registry, name, &wp_viewporter_interface, version);
    }
    else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name))
    {
        // zwlr_layer_shell_v1
        if (version > 4)
            version = 4;

        zwlr_layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version);
    }
    else if (!strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name))
    {
        // zwlr_foreign_toplevel_manager_v1
        if (version > 3)
            version = 3;

        zwlr_toplevel_manager = wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface, version);
        zwlr_foreign_toplevel_manager_v1_add_listener(zwlr_toplevel_manager, &ToplevelManager_listener, NULL);
    }
}

static void RegistryGlobalRemove(
    void *data,
    struct wl_registry *wl_registry,
    uint32_t name
)
{
    // check if name belongs to wl_output and remove
    struct Desktop *desktop = NULL;
    struct Desktop output_query = { .wl_name = name };
    
    if ((desktop = HashSet_Find(&global_desktops, &output_query)))
    {
        Debug_Printf(
            "output '%s' removed\b",
            desktop->output_name ? desktop->output_name : str_null
        );

        Desktop_Destroy(desktop);
    }
}

static const struct wl_registry_listener registry_listener =
{
    .global        = RegistryGlobal,
    .global_remove = RegistryGlobalRemove,
};

#pragma endregion

#pragma region InitializationAndCleanup

static void Init(void)
{
    // wl display and fd
    if (!(wl_display = wl_display_connect(NULL)))
    {
        fprintf(stderr, "failed connecting to Wayland server: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    display_fd = wl_display_get_fd(wl_display);

    // fd for app events
    if (pipe(event_pipe_fds) == -1)
    {
        fprintf(stderr, "failed to create pipe for local events: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fcntl(event_pipe_fds[0], F_SETFL, fcntl(event_pipe_fds[0], F_GETFL) | O_NONBLOCK);

    // FreeType initialization
    // TODO: use fontconfig for font loading?
    if (!FTEssentials_InitState(&global_ft_state, fontpath))
    {
        fprintf(stderr, "failed to init FreeType state: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    FTWordWrapLineBuffer_Init(&global_ft_line_buffer);

    // collection initialization
    HashSet_Init(&global_desktops);

    // icon init
    Icon_Init();

    Keyboard_Init(&global_keyboard);
    Pointer_Init(&global_pointer);

    // bind registries
    wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &registry_listener, NULL);
    wl_display_roundtrip(wl_display);

    // roundtrip for callbacks
    wl_display_roundtrip(wl_display);
    wl_display_roundtrip(wl_display);
    wl_display_roundtrip(wl_display);
}

static void Cleanup(void)
{
    struct HashSet_Iter iter_hashset = { };
    struct LinkedList_Iter iter_linkedlist = { };
    struct LinkedList_Node *desktop_node = NULL;
    struct Desktop *desktop = NULL;
    struct LinkedList_Node *toplevel_handle_node = NULL;
    struct ToplevelHandle *toplevel_handle = NULL;

    // desktops
    HashSet_IterInit(&iter_hashset, &global_desktops);
    while ((desktop = HashSet_IterDel(&iter_hashset)))
        Desktop_Destroy(desktop);
    HashSet_Cleanup(&global_desktops);

    LinkedList_IterInit(&iter_linkedlist, &global_desktops_created);
    while ((desktop_node = LinkedList_IterDel(&iter_linkedlist)))
    {
        Desktop_Destroy(desktop_node->elem);
        LinkedList_DestroyNode(desktop_node);
    }

    // toplevel handles
    LinkedList_IterInit(&iter_linkedlist, &global_toplevel_handles);
    while ((toplevel_handle_node = LinkedList_IterNext(&iter_linkedlist)))
        ToplevelHandle_Destroy(toplevel_handle_node->elem);

    LinkedList_IterInit(&iter_linkedlist, &global_toplevel_handles_closed);
    while ((toplevel_handle_node = LinkedList_IterNext(&iter_linkedlist)))
        ToplevelHandle_Destroy(toplevel_handle_node->elem);

    // event pipe fds
    close(event_pipe_fds[0]);
    close(event_pipe_fds[1]);

    // keyboard & mouse
    Keyboard_Cleanup(&global_keyboard);
    Pointer_Cleanup(&global_pointer);

    // wl objects
    if (zwlr_layer_shell)
        zwlr_layer_shell_v1_destroy(zwlr_layer_shell);
    if (zwlr_toplevel_manager)
        zwlr_foreign_toplevel_manager_v1_destroy(zwlr_toplevel_manager);

    if (wp_viewporter)
        wp_viewporter_destroy(wp_viewporter);
    if (wl_shm)
        wl_shm_destroy(wl_shm);
    if (wl_compositor)
        wl_compositor_destroy(wl_compositor);

    if (wp_cursor_shape_manager)
        wp_cursor_shape_manager_v1_destroy(wp_cursor_shape_manager);

    if (wl_seat)
        wl_seat_destroy(wl_seat);

    wl_display_disconnect(wl_display);
}

#pragma endregion

static void FinishDesktopCreation(void)
{
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *desktop_node = NULL;
    struct Desktop *desktop = NULL;

    wl_display_roundtrip(wl_display);

    LinkedList_IterInit(&iter, &global_desktops_created);
    while ((desktop_node = LinkedList_IterDel(&iter)))
    {
        desktop = desktop_node->elem;

        DesktopSurface_Init(&desktop->desktop_surface, desktop->wl_output);
        DoubleBufferedSurface_SetName(&desktop->desktop_surface.surface, desktop->output_name);

    #if DEBUG_TEST == 1
        TestPatternDrawOnDesktopLayerSurface(&desktop->desktop_layer_surface);
        TestSWRenderOnDesktopLayerSurface(&desktop->desktop_layer_surface);
    #endif

        DoubleBufferedSurface_AddDamage(&desktop->desktop_surface.surface, NULL);
        DoubleBufferedSurface_TryCommit(&desktop->desktop_surface.surface);

        Desktop_AddDamage(desktop, 0, 0, desktop->w, desktop->h);

        wl_display_roundtrip(wl_display);

        LinkedList_DelNode(desktop_node);

        desktop->surface_created = true;
    }
}

static void UpdateDesktops(void)
{
    struct HashSet_Iter iter = { };
    struct Desktop *desktop = NULL;
    
    HashSet_IterInit(&iter, &global_desktops);
    while ((desktop = HashSet_IterNext(&iter)))
    {
        Desktop_Update(desktop);
        Desktop_Draw(desktop);
        Desktop_TryCommit(desktop);
    }
}

static void FinishToplevelHandlerCleanup(void)
{
    struct LinkedList_Iter iter = { };
    struct LinkedList_Node *toplevel_handle_node = NULL;
    struct ToplevelHandle *toplevel_handle = NULL;

    LinkedList_IterInit(&iter, &global_toplevel_handles_closed);
    while ((toplevel_handle_node = LinkedList_IterDel(&iter)))
        ToplevelHandle_Destroy(toplevel_handle_node->elem);
}

bool DispatchEvents(void)
{
    struct pollfd pollfds[2] =
    {
        { display_fd,        POLLIN },
        { event_pipe_fds[0], POLLIN },
    };

    // --------------------------------- poll ---------------------------------
    wl_display_prepare_read(wl_display);

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

    // ---------------------------- local event fd ----------------------------
    if (pollfds[1].revents & POLLIN)
    {
        // desktop creation
        FinishDesktopCreation();

        // clear
        char discard_buf[128];
        while (read(event_pipe_fds[0], discard_buf, sizeof(discard_buf)) > 0);
    }

    // ------------------------------------------------------------------------
    UpdateDesktops();

    FinishToplevelHandlerCleanup();

    // ------------------------------------------------------------------------
    wl_display_flush(wl_display);

    return true;
}

int main(int argc, char **argv)
{
    signal(SIGINT, SignalHandlerSIGINT);

    Init();

    while (DispatchEvents() && !global_interrupted);

    Cleanup();

    return EXIT_SUCCESS;
}
