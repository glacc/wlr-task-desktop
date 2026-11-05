#include "ToplevelHandle.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include "Utility.h"

#include "AppState.h"
#include "Desktop.h"

struct ToplevelHandle *ToplevelHandle_Create(void)
{
    struct ToplevelHandle *toplevel_handle = calloc(1, sizeof(*toplevel_handle));

    if (!toplevel_handle)
    {
        fprintf(stderr, "failed to allocate memory for struct ToplevelHandle: %s", strerror(errno));
        return NULL;
    }

    if (!HashSet_Init(&toplevel_handle->child))
    {
        fprintf(stderr, "failed to initialize child ref HashSet in struct ToplevelHandle: %s", strerror(errno));
        free(toplevel_handle);
        return NULL;
    }

    if (!HashSet_Init(&toplevel_handle->is_inside_desktops))
    {
        fprintf(stderr, "failed to initialize desktop ref HashSet in struct ToplevelHandle: %s", strerror(errno));
        HashSet_Cleanup(&toplevel_handle->child);
        free(toplevel_handle);
        return NULL;
    }

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

    struct HashSet_Iter iter = { };
    
    // unref child and cleanup
    if (HashSet_IterInit(&iter, &toplevel_handle->child))
    {
        struct ToplevelHandle *child_handle = NULL;

        while ((child_handle = HashSet_IterDel(&iter)))
            child_handle->parent = NULL;
    }
    HashSet_Cleanup(&toplevel_handle->child);

    // unref from Output and cleanup
    if (HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktops))
    {
        struct Desktop *desktop = NULL;

        while ((desktop = HashSet_IterDel(&iter)))
            HashSet_Del(&desktop->toplevel_handles, toplevel_handle);
    }
    HashSet_Cleanup(&toplevel_handle->is_inside_desktops);

    // destroy zwlr handle
    if (toplevel_handle->toplevel_handle)
        zwlr_foreign_toplevel_handle_v1_destroy(toplevel_handle->toplevel_handle);

    free(toplevel_handle);
}

static void ToplevelHandle_PrintTitleAndAppID(struct ToplevelHandle *toplevel_handle)
{
    printf("%s (%s)", toplevel_handle->title, toplevel_handle->app_id);
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

static void ToplevelHandle_Title(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    const char *title
)
{
    struct ToplevelHandle *toplevel_handle = data;

    Utility_TryReplaceWithAnotherString(&toplevel_handle->title, title);

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
        toplevel_handle->iconify_to_desktop = desktop;

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
        toplevel_handle->iconify_to_desktop = desktop;
    else if (toplevel_handle->is_inside_desktops.count == 0)
        toplevel_handle->iconify_to_desktop = NULL;

    ToplevelHandle_PrintIsInsideWhichOutput(toplevel_handle);
}

static void ToplevelHandle_State(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_array *states
)
{
    struct ToplevelHandle *toplevel_handle = data;

    toplevel_handle->maximized = false;
    toplevel_handle->minimized = false;
    toplevel_handle->activated = false;
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
    printf(
        "\n\tmaximized(%d), minimized(%d), activated(%d), fullscreen(%d)\n",
        toplevel_handle->maximized,
        toplevel_handle->minimized,
        toplevel_handle->activated,
        toplevel_handle->fullscreen
    );
}

static void ToplevelHandle_Done(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct ToplevelHandle *toplevel_handle = data;
    
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

    // unref if parent exist
    struct ToplevelHandle *parent = toplevel_handle->parent;
    if (parent)
        HashSet_Del(&parent->child, toplevel_handle);

    struct HashSet_Iter iter = { };

    // unref child
    if (HashSet_IterInit(&iter, &toplevel_handle->child))
    {
        struct ToplevelHandle *child = NULL;

        while ((child = HashSet_IterDel(&iter)))
            child->parent = NULL;
    }
    HashSet_Cleanup(&toplevel_handle->child);

    // unref between Outputs
    if (HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktops))
    {
        struct Desktop *desktop = NULL;

        while ((desktop = HashSet_IterDel(&iter)))
            HashSet_Del(&desktop->toplevel_handles, &desktop);
    }
    HashSet_Cleanup(&toplevel_handle->is_inside_desktops);

    // unref this from toplevel manager
    HashSet_Del(&toplevel_handle->app_state->toplevel_handles, toplevel_handle);

    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    puts(" closed");

    // deconstruct
    ToplevelHandle_Destroy(toplevel_handle);
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
    if (!HashSet_Add(&parent_handle->child, toplevel_handle, NULL))
    {
        fprintf(stderr, "failed to append child handle to parent.\n");
        return;
    }

    toplevel_handle->parent = parent_handle;

    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    printf(" is child of:\n\t");
    ToplevelHandle_PrintTitleAndAppID(parent_handle);
    putchar('\n');
}

const struct zwlr_foreign_toplevel_handle_v1_listener ToplevelHandle_wl_listener =
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
