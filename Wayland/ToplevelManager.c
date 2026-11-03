#include "ToplevelManager.h"

#include <malloc.h>
#include "ToplevelHandle.h"

bool ToplevelManager_Init(struct ToplevelManager *toplevel_manager)
{
    if (!toplevel_manager)
        return false;

    if (!Vector_Init(&toplevel_manager->handles, sizeof(struct ToplevelHandle *)))
        return false;

    return true;
}

void ToplevelManager_Cleanup(struct ToplevelManager *toplevel_manager)
{
    if (!toplevel_manager)
        return;

    if (toplevel_manager->handles.array)
    {
        for (int i = 0; i < toplevel_manager->handles.count; i++)
        {
            struct ToplevelHandle *handle = *(struct ToplevelHandle **)Vector_Index(&toplevel_manager->handles, i);
            ToplevelHandle_Cleanup(handle);
            free(handle);
        }
    }

    Vector_Free(&toplevel_manager->handles);
}

static void ToplevelManagerListener_Toplevel(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct ToplevelManager *toplevel_manager = data;

    struct ToplevelHandle *handle = calloc(1, sizeof(struct ToplevelHandle));

    if (!handle)
    {
        fprintf(stderr, "failed to allocate memory for ToplevelHandle\n");
        return;
    }

    if (!Vector_Add(&toplevel_manager->handles, &handle))
    {
        free (handle);
        fprintf(stderr, "failed to append ToplevelHandle\n");
        return;
    }

    if (!Vector_Init(&handle->child, sizeof(struct ToplevelHandle *)))
    {
        free(handle);
        fprintf(stderr, "failed to init child Vector for ToplevelHandle");
        return;
    }

    handle->toplevel_handle = zwlr_toplevel_handle;
    handle->manager = toplevel_manager;

    zwlr_foreign_toplevel_handle_v1_add_listener(handle->toplevel_handle, &ToplevelHandle_wl_listener, handle);
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