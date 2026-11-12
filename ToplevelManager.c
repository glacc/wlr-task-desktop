/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#include "ToplevelManager.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include "Globals.h"
#include "ToplevelHandle.h"

static void ToplevelManagerListener_Toplevel(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    // construct
    struct ToplevelHandle *toplevel_handle = ToplevelHandle_Create();

    if (!toplevel_handle)
    {
        fprintf(stderr, "failed to construct struct ToplevelHandle\n");
        return;
    }

    // put into toplevel_handles
    if (!HashSet_Add(&global_toplevel_handles, toplevel_handle, NULL))
    {
        fprintf(stderr, "failed to add struct ToplevelHandle into ToplevelManager handle HashSet: %s\n", strerror(errno));
        ToplevelHandle_Destroy(toplevel_handle);
        return;
    }
    
    toplevel_handle->toplevel_handle = zwlr_toplevel_handle;

    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel_handle->toplevel_handle, &ToplevelHandle_listener, toplevel_handle);
}

static void ToplevelManagerListener_Finished(
    void *data,
    struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager
)
{
    zwlr_foreign_toplevel_manager_v1_destroy(zwlr_toplevel_manager);
}

const struct zwlr_foreign_toplevel_manager_v1_listener ToplevelManager_listener =
{
    .toplevel = ToplevelManagerListener_Toplevel,
    .finished = ToplevelManagerListener_Finished,
};