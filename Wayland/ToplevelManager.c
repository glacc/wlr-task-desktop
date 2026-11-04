#include "ToplevelManager.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include "ToplevelHandle.h"

bool ToplevelManager_Init(struct ToplevelManager *toplevel_manager)
{
    if (!toplevel_manager)
        return false;

    if (!HashSet_Init(&toplevel_manager->handles))
        return false;

    return true;
}

void ToplevelManager_Cleanup(struct ToplevelManager *toplevel_manager)
{
    if (!toplevel_manager)
        return;

    struct HashSet_Iter iter = { };

    if (HashSet_IterInit(&iter, &toplevel_manager->handles))
    {
        struct ToplevelHandle *handle = NULL;

        while ((handle = HashSet_IterDel(&iter)))
        {
            ToplevelHandle_Destroy(handle);
        }
    }

    HashSet_Cleanup(&toplevel_manager->handles);
}

static void ToplevelManagerListener_Toplevel(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct ToplevelManager *toplevel_manager = data;

    // construct
    struct ToplevelHandle *toplevel_handle = ToplevelHandle_New();

    if (!toplevel_handle)
    {
        fprintf(stderr, "failed to construct struct ToplevelHandle\n");
        return;
    }

    // put into ToplevelManager
    if (!HashSet_Add(&toplevel_manager->handles, toplevel_handle, NULL))
    {
        fprintf(stderr, "failed to add struct ToplevelHandle into ToplevelManager handle HashSet: %s\n", strerror(errno));
        ToplevelHandle_Destroy(toplevel_handle);
        return;
    }

    toplevel_handle->toplevel_handle = zwlr_toplevel_handle;
    toplevel_handle->manager = toplevel_manager;

    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel_handle->toplevel_handle, &ToplevelHandle_wl_listener, toplevel_handle);
}

static void ToplevelManagerListener_Finished(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager
)
{
    struct ToplevelManager *toplevel_manager = data;

    zwlr_foreign_toplevel_manager_v1_destroy(toplevel_manager->toplevel_manager);
}

const struct zwlr_foreign_toplevel_manager_v1_listener ToplevelManager_wl_listener =
{
    .toplevel = ToplevelManagerListener_Toplevel,
    .finished = ToplevelManagerListener_Finished,
};