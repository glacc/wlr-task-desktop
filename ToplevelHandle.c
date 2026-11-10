#include "ToplevelHandle.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include "Utility.h"

#include "Globals.h"
#include "Desktop.h"

#include "Icon.h"

#include "SWRender/SWRenderExtImage.h"

int ToplevelHandle_debug_verbosity = 1;

#pragma region PrintHandleInfo

static void ToplevelHandle_PrintTitleAndAppID(struct ToplevelHandle *toplevel_handle)
{
    printf("'%s' (%s)", toplevel_handle->title, toplevel_handle->app_id);
}

void ToplevelHandle_PrintIsInsideWhichOutput(struct ToplevelHandle *toplevel_handle)
{
    if (!toplevel_handle)
        return;

    struct HashSet_Iter iter = { };
    
    if (HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktops))
    {
        struct Desktop *desktop = NULL;

        ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
        printf(" is inside desktop of output(s):\n\t");

        while ((desktop = HashSet_IterNext(&iter)))
            printf("%s, ", desktop->name);

        putchar('\n');
    }
}

void ToplevelHandle_PrintIconifyToDesktop(struct ToplevelHandle *toplevel_handle)
{
    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    printf(
        "is inside desktop of output: %s\n",
        toplevel_handle->iconify_to_desktop ? toplevel_handle->iconify_to_desktop->name : "NULL"
    );
}

#pragma endregion

#pragma region WaylandCallbacks

static void ToplevelHandle_Title(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    const char *title
)
{
    struct ToplevelHandle *toplevel_handle = data;

    Utility_TryReplaceWithAnotherString(&toplevel_handle->title, title);
    
    toplevel_handle->desktop_entry.redraw = true;

    if (ToplevelHandle_debug_verbosity < 1)
        return;

    printf("title: %s\n", title);
}

static void ToplevelHandle_AppID(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    const char *app_id
)
{
    struct ToplevelHandle *toplevel_handle = data;

    Utility_TryReplaceWithAnotherString(&toplevel_handle->app_id, app_id);

    if (toplevel_handle->app_id)
    {
        // TODO: debugging....
        char *path = NULL;

        Icon_FindAppIconPath(toplevel_handle->app_id, 64, 1, &path);

        if (path)
        {
            SWRenderBuffer icon_orig_buf = { };

            if (SWRender_LoadImageFromFileToBuffer(path, &icon_orig_buf) < 0)
                fprintf(stderr, "failed to load icon '%s' into buffer: %s\n", path, strerror(errno));
            else
            {
                toplevel_handle->desktop_entry.icon_buf.w = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_X;
                toplevel_handle->desktop_entry.icon_buf.h = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_Y;
                SWRender_DupSWScaledToBuffer(&icon_orig_buf, &toplevel_handle->desktop_entry.icon_buf);

                SWRender_BufferFree(&icon_orig_buf);
            }

            printf("icon found: %s\n", path);

            free(path);
        }
    }

    if (ToplevelHandle_debug_verbosity < 1)
        return;

    printf("app_id: %s\n", app_id);
}

static void ToplevelHandle_OutputEnter(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_output *wl_output
)
{
    struct ToplevelHandle *toplevel_handle = data;
    struct Desktop *desktop = wl_output_get_user_data(wl_output);

    if (!desktop)
        return;

    // ref this->desktop
    HashSet_Add(&toplevel_handle->is_inside_desktops, desktop, NULL);
    // ref desktop->this
    HashSet_Add(&desktop->toplevel_handles, toplevel_handle, NULL);

    // determine its desktop to iconify
    if (toplevel_handle->is_inside_desktops.count == 1)
    {
        toplevel_handle->iconify_to_desktop = desktop;
        toplevel_handle->desktop_entry.has_not_been_minimized_since_last_desktop_change = !toplevel_handle->minimized;

        if (ToplevelHandle_debug_verbosity >= 1)
            ToplevelHandle_PrintIconifyToDesktop(toplevel_handle);
    }

    if (ToplevelHandle_debug_verbosity < 2)
        return;

    ToplevelHandle_PrintIsInsideWhichOutput(toplevel_handle);
}

static void ToplevelHandle_OutputLeave(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_output *wl_output
)
{
    struct ToplevelHandle *toplevel_handle = data;
    struct Desktop *desktop = wl_output_get_user_data(wl_output);

    if (!desktop)
        return;

    // unref this->desktop
    HashSet_Del(&toplevel_handle->is_inside_desktops, desktop);
    // unref desktop->this
    HashSet_Del(&desktop->toplevel_handles, toplevel_handle);

    // determine its desktop to iconify
    if (toplevel_handle->is_inside_desktops.count == 1)
    {
        struct Desktop *new_iconify_to_desktop = NULL;

        struct HashSet_Iter iter = { };
        if (HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktops))
            new_iconify_to_desktop = HashSet_IterNext(&iter);

        toplevel_handle->iconify_to_desktop = new_iconify_to_desktop;

        toplevel_handle->desktop_entry.has_not_been_minimized_since_last_desktop_change = !toplevel_handle->minimized;

        if (ToplevelHandle_debug_verbosity >= 1)
            ToplevelHandle_PrintIconifyToDesktop(toplevel_handle);
    }
    else if (toplevel_handle->is_inside_desktops.count == 0)
        toplevel_handle->iconify_to_desktop = NULL;

    if (ToplevelHandle_debug_verbosity < 2)
        return;

    ToplevelHandle_PrintIsInsideWhichOutput(toplevel_handle);
}

static void ToplevelHandle_State(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_array *states
)
{
    struct ToplevelHandle *toplevel_handle = data;

    bool maximized_old  = toplevel_handle->maximized;
    bool minimized_old  = toplevel_handle->minimized;
    bool activated_old  = toplevel_handle->activated;
    bool fullscreen_old = toplevel_handle->fullscreen;

    toplevel_handle->maximized  = false;
    toplevel_handle->minimized  = false;
    toplevel_handle->activated  = false;
    toplevel_handle->fullscreen = false;

    uint32_t *state;
    wl_array_for_each(state, states)
    {
        switch (*state)
        {
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
                toplevel_handle->maximized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
                toplevel_handle->minimized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
                toplevel_handle->activated = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
                toplevel_handle->fullscreen = true;
                break;
        }
    }
    
    printf("state of ");
    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    printf("\n\t");

    // if state maximized changed
    if (maximized_old != toplevel_handle->maximized)
        printf("maximized(%d -> %d), ", maximized_old, toplevel_handle->maximized);
    else
        printf("maximized(%d), ", toplevel_handle->maximized);

    // if state minimized changed
    if (minimized_old != toplevel_handle->minimized)
    {
        printf("minimized(%d -> %d), ", minimized_old, toplevel_handle->minimized);

        toplevel_handle->desktop_entry.visible = toplevel_handle->minimized;

        if (toplevel_handle->minimized)
        {
            toplevel_handle->desktop_entry.has_not_been_minimized_since_last_desktop_change = false;
            
            if (toplevel_handle->desktop_entry.recalc_position_when_minimize)
                toplevel_handle->desktop_entry.recalc_position = true;
        }

        toplevel_handle->desktop_entry.redraw = true;
    }
    else
    {
        printf("minimized(%d), ", toplevel_handle->minimized);
    }

    if (ToplevelHandle_debug_verbosity < 1)
        return;

    // if state activated changed
    if (activated_old != toplevel_handle->activated)
        printf("activated(%d -> %d), ", activated_old, toplevel_handle->activated);
    else
        printf("activated(%d), ", toplevel_handle->activated);

    // if state fullscreen changed
    if (fullscreen_old != toplevel_handle->fullscreen)
        printf("fullscreen(%d -> %d)\n", fullscreen_old, toplevel_handle->fullscreen);
    else
        printf("fullscreen(%d)\n", toplevel_handle->fullscreen);
}

static void ToplevelHandle_Done(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct ToplevelHandle *toplevel_handle = data;

    if (ToplevelHandle_debug_verbosity < 2)
        return;
    
    printf("update done: ");
    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    putchar('\n');
}

static void ToplevelHandle_Closed(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct ToplevelHandle *toplevel_handle = data;

    if (ToplevelHandle_debug_verbosity >= 1)
    {
        ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
        puts(" closed");
    }

    toplevel_handle->desktop_entry.redraw  = true;
    toplevel_handle->desktop_entry.visible = false;

    // unref this from toplevel manager
    HashSet_Del(&global_toplevel_handles, toplevel_handle);

    // deconstruct pending
    Vector_Add(&global_toplevel_handle_to_destroy, &toplevel_handle);

    //// deconstruct
    //ToplevelHandle_Destroy(toplevel_handle);
}

static void ToplevelHandle_Parent(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct zwlr_foreign_toplevel_handle_v1 *parent
)
{
    struct ToplevelHandle *toplevel_handle = data;

    if (!parent)
        return;

    struct ToplevelHandle *parent_handle = zwlr_foreign_toplevel_handle_v1_get_user_data(parent);

    //if (!Vector_Add(&parent_handle->child, &toplevel_handle))
    if (!HashSet_Add(&parent_handle->children, toplevel_handle, NULL))
    {
        fprintf(stderr, "failed to append child handle to parent.\n");
        return;
    }

    toplevel_handle->parent = parent_handle;

    if (ToplevelHandle_debug_verbosity < 2)
        return;

    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    printf(" is child of:\n\t");
    ToplevelHandle_PrintTitleAndAppID(parent_handle);
    putchar('\n');
}

const struct zwlr_foreign_toplevel_handle_v1_listener ToplevelHandle_listener =
{
    .title        = ToplevelHandle_Title,
    .app_id       = ToplevelHandle_AppID,
    .output_enter = ToplevelHandle_OutputEnter,
    .output_leave = ToplevelHandle_OutputLeave,
    .state        = ToplevelHandle_State,
    .done         = ToplevelHandle_Done,
    .closed       = ToplevelHandle_Closed,
    .parent       = ToplevelHandle_Parent,
};

#pragma endregion

struct ToplevelHandle *ToplevelHandle_Create(void)
{
    struct ToplevelHandle *toplevel_handle = calloc(1, sizeof(*toplevel_handle));

    if (!toplevel_handle)
    {
        fprintf(stderr, "failed to allocate memory for struct ToplevelHandle: %s", strerror(errno));
        return NULL;
    }

    if (!HashSet_Init(&toplevel_handle->children))
    {
        fprintf(stderr, "failed to initialize child ref HashSet in struct ToplevelHandle: %s", strerror(errno));
        free(toplevel_handle);
        return NULL;
    }

    if (!HashSet_Init(&toplevel_handle->is_inside_desktops))
    {
        fprintf(stderr, "failed to initialize desktop ref HashSet in struct ToplevelHandle: %s", strerror(errno));
        HashSet_Cleanup(&toplevel_handle->children);
        free(toplevel_handle);
        return NULL;
    }

    toplevel_handle->desktop_entry.has_position = false;

    toplevel_handle->desktop_entry.visible = false;

    toplevel_handle->desktop_entry.has_not_been_minimized_since_last_desktop_change = true;

    toplevel_handle->desktop_entry.recalc_position_when_minimize = true;
    toplevel_handle->desktop_entry.recalc_position = true;

    toplevel_handle->desktop_entry.redraw = false;

    return toplevel_handle;
}

void ToplevelHandle_Destroy(struct ToplevelHandle *toplevel_handle)
{
    if (!toplevel_handle)
        return;

    // free memory for strings
    if (toplevel_handle->title)
        free(toplevel_handle->title);

    if (toplevel_handle->app_id)
        free(toplevel_handle->app_id);

    // unref if parent exist
    struct ToplevelHandle *parent = toplevel_handle->parent;
    if (parent)
        HashSet_Del(&parent->children, toplevel_handle);

    struct HashSet_Iter iter = { };
    
    // unref child and cleanup
    if (HashSet_IterInit(&iter, &toplevel_handle->children))
    {
        struct ToplevelHandle *child_handle = NULL;

        while ((child_handle = HashSet_IterDel(&iter)))
            child_handle->parent = NULL;
    }
    HashSet_Cleanup(&toplevel_handle->children);

    // unref from Output and cleanup
    if (HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktops))
    {
        struct Desktop *desktop = NULL;

        while ((desktop = HashSet_IterDel(&iter)))
        {
            // TODO: redraw to remove icon of closed application

            HashSet_Del(&desktop->toplevel_handles, toplevel_handle);

            if (desktop->selected_handle == toplevel_handle)
                desktop->selected_handle = NULL;
        }
    }
    HashSet_Cleanup(&toplevel_handle->is_inside_desktops);

    //// unref this from toplevel manager
    //HashSet_Del(&global_toplevel_handles, toplevel_handle);

    // destroy zwlr handle
    if (toplevel_handle->toplevel_handle)
        zwlr_foreign_toplevel_handle_v1_destroy(toplevel_handle->toplevel_handle);

    SWRender_BufferFree(&toplevel_handle->desktop_entry.icon_buf);

    free(toplevel_handle);
}
