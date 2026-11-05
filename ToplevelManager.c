#include "ToplevelManager.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include "AppState.h"
#include "ToplevelHandle.h"

static void ToplevelManagerListener_Toplevel(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct AppState *app_state = data;

    // construct
    struct ToplevelHandle *toplevel_handle = ToplevelHandle_Create();

    if (!toplevel_handle)
    {
        fprintf(stderr, "failed to construct struct ToplevelHandle\n");
        return;
    }

    // put into toplevel_handles
    if (!HashSet_Add(&app_state->toplevel_handles, toplevel_handle, NULL))
    {
        fprintf(stderr, "failed to add struct ToplevelHandle into ToplevelManager handle HashSet: %s\n", strerror(errno));
        ToplevelHandle_Destroy(toplevel_handle);
        return;
    }

    toplevel_handle->toplevel_handle = zwlr_toplevel_handle;
    toplevel_handle->app_state = app_state;

    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel_handle->toplevel_handle, &ToplevelHandle_wl_listener, toplevel_handle);
}

static void ToplevelManagerListener_Finished(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager
)
{
    struct AppState *app_state = data;

    zwlr_foreign_toplevel_manager_v1_destroy(app_state->toplevel_manager);
}

const struct zwlr_foreign_toplevel_manager_v1_listener ToplevelManager_wl_listener =
{
    .toplevel = ToplevelManagerListener_Toplevel,
    .finished = ToplevelManagerListener_Finished,
};