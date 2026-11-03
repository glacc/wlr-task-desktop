#include "ToplevelHandle.h"

#include <stdio.h>
#include <malloc.h>
#include <string.h>

void ToplevelHandle_Cleanup(struct ToplevelHandle *toplevel_handle)
{
    if (!toplevel_handle)
        return;

    if (toplevel_handle->title)
        free(toplevel_handle->title);

    if (toplevel_handle->app_id)
        free(toplevel_handle->app_id);

    if (toplevel_handle->toplevel_handle)
        zwlr_foreign_toplevel_handle_v1_destroy(toplevel_handle->toplevel_handle);
}

static void ToplevelHandle_PrintTitleAndAppID(struct ToplevelHandle *toplevel_handle)
{
    printf(
        "%s (%s)",
        toplevel_handle->title  ? toplevel_handle->title  : "",
        toplevel_handle->app_id ? toplevel_handle->app_id : ""
    );
}

static void ToplevelHandle_Title(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    const char *title
)
{
    struct ToplevelHandle *toplevel_handle = data;

    if (toplevel_handle->title)
        free(toplevel_handle->title);

    if (!(toplevel_handle->title = malloc(strlen(title) + 1)))
    {
        fprintf(stderr, "failed to allocate memory for title\n");
        return;
    }
    
    strcpy(toplevel_handle->title, title);

    printf("title: %s\n", title);
}

static void ToplevelHandle_AppID(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    const char *app_id
)
{
    struct ToplevelHandle *toplevel_handle = data;

    if (toplevel_handle->app_id)
        free(toplevel_handle->app_id);

    if (!(toplevel_handle->app_id = malloc(strlen(app_id) + 1)))
    {
        fprintf(stderr, "failed to allocate memory for app_id\n");
        return;
    }
    
    strcpy(toplevel_handle->app_id, app_id);

    printf("app_id: %s\n", app_id);
}

static void ToplevelHandle_OutputEnter(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_output *output
)
{

}

static void ToplevelHandle_OutputLeave(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_output *output
)
{
    
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

    if (parent && parent->child.array)
    {
        for (int i = 0; i < parent->child.count; i++)
        {
            struct ToplevelHandle *child_of_parent = *(struct ToplevelHandle **)Vector_Index(&parent->child, i);

            if (child_of_parent == toplevel_handle)
            {
                Vector_RemoveAt(&parent->child, i);
                break;
            }
        }
    }

    // cleanup this
    ToplevelHandle_Cleanup(toplevel_handle);

    for (int i = 0; i < toplevel_handle->manager->handles.count; i++)
    {
        struct ToplevelHandle *handle = *(struct ToplevelHandle **)Vector_Index(&toplevel_handle->manager->handles, i);

        if (handle == toplevel_handle)
        {
            Vector_RemoveAt(&toplevel_handle->manager->handles, i);
            free(handle);
            break;
        }
    }

    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    puts(" closed");
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

    if (!Vector_Add(&parent_handle->child, &toplevel_handle))
    {
        fprintf(stderr, "failed to append child handle to parent.\n");
        return;
    }

    toplevel_handle->parent = parent_handle;

    printf("parent of ");
    ToplevelHandle_PrintTitleAndAppID(toplevel_handle);
    putchar(':');
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
