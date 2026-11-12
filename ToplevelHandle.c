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

#include "ToplevelHandle.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>

#include "Globals.h"
#include "Desktop.h"

#include "Icon.h"

#include "SWRender/SWRenderExtImage.h"

#include "Debug.h"

#pragma region Debugging

static const char str_null[] = "NULL";

static void ToplevelHandle_DebugPrintfWithTitleAndAppID(struct ToplevelHandle *toplevel_handle, const char *format, ...)
{
    va_list args;

    printf(
        "'%s' (%s)",
        toplevel_handle->title  ? toplevel_handle->title  : str_null,
        toplevel_handle->app_id ? toplevel_handle->app_id : str_null
    );

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void ToplevelHandle_DebugPrintDesktops(struct ToplevelHandle *toplevel_handle)
{
    struct HashSet_Iter iter = { };
    struct Desktop *desktop = NULL;

    ToplevelHandle_DebugPrintfWithTitleAndAppID(toplevel_handle, " is now in output:\n\t");

    HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktop);
    while ((desktop = HashSet_IterNext(&iter)))
        printf("%s, ", desktop->output_name);

    putchar('\n');
}

#pragma endregion

static void ToplevelHandle_ChangeDesktopToIconify(
    struct ToplevelHandle *toplevel_handle,
    struct Desktop *desktop_to_iconify
)
{
    if (toplevel_handle->last_iconified_to)
        Desktop_AddDamageToplevelHandle(toplevel_handle->last_iconified_to, toplevel_handle);
    
    toplevel_handle->desktop_entry.has_position = false;

    toplevel_handle->desktop_entry.has_not_been_minimized_since_last_desktop_change = !toplevel_handle->minimized;

    toplevel_handle->desktop_entry.recalc_position_when_minimize = true;
    toplevel_handle->desktop_entry.recalc_position = true;

    toplevel_handle->iconify_to_desktop = desktop_to_iconify;

    if (toplevel_handle->desktop_entry.visible)
    {
        toplevel_handle->desktop_entry.redraw = true;
        toplevel_handle->last_iconified_to = desktop_to_iconify;
    }
}

#pragma region ToplevelHandleListeners

static void ToplevelHandle_Title(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    const char *title
)
{
    struct ToplevelHandle *toplevel_handle = data;
    bool title_change = false;

    if (toplevel_handle->title)
    {
        title_change = true;
        Debug_Printf(
            "app_id '%s' title changed from '%s'\n",
            toplevel_handle->app_id ? toplevel_handle->app_id : str_null,
            toplevel_handle->title
        );

        free(toplevel_handle->title);
    }

    toplevel_handle->title = strdup(title);

    if (title_change)
        Debug_Printf("\t-> '%s'\n", toplevel_handle->title);
    else
        Debug_Printf("title: '%s'\n", toplevel_handle->title);
    
    toplevel_handle->desktop_entry.redraw = true;
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

    toplevel_handle->app_id = strdup(app_id);
    Debug_Printf("app_id '%s'\n", toplevel_handle->app_id);

    if (!toplevel_handle->app_id)
        return;

    // get icon
    // TODO: debugging....
    char *path = NULL;
    SWRenderBuffer new_icon_buffer = { };

    Icon_FindAppIconPath(toplevel_handle->app_id, 64, 1, &path);

    if (!path)
        return;

    if (SWRender_LoadImageFromFileToBuffer(path, &new_icon_buffer) < 0)
        fprintf(stderr, "failed to load icon '%s' into buffer: %s\n", path, strerror(errno));
    else
    {
        toplevel_handle->desktop_entry.icon_buf.w = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_X;
        toplevel_handle->desktop_entry.icon_buf.h = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_Y;
        SWRender_DupSWScaledToBuffer(&new_icon_buffer, &toplevel_handle->desktop_entry.icon_buf);
        SWRender_BufferFree(&new_icon_buffer);
    }

    Debug_Printf("icon '%s'\n", path);

    free(path);
}

static void ToplevelHandle_OutputEnter(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_output *wl_output
)
{
    struct ToplevelHandle *toplevel_handle = data;
    struct Desktop *desktop = wl_output_get_user_data(wl_output);

    // ref this<->desktop
    HashSet_Add(&toplevel_handle->is_inside_desktop, desktop, NULL);
    LinkedList_InsertElemTail(&desktop->toplevel_handles, toplevel_handle);

    // determine which desktop to iconify
    if (toplevel_handle->is_inside_desktop.count == 1)
    {
        ToplevelHandle_ChangeDesktopToIconify(toplevel_handle, desktop);

        ToplevelHandle_DebugPrintfWithTitleAndAppID(
            toplevel_handle,
            " will now be iconified to output: %s\n",
            desktop->output_name ? desktop->output_name : str_null
        );
    }
    else
        ToplevelHandle_DebugPrintDesktops(toplevel_handle);
}

static void ToplevelHandle_OutputLeave(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct wl_output *wl_output
)
{
    struct ToplevelHandle *toplevel_handle = data;
    struct Desktop *desktop = wl_output_get_user_data(wl_output);

    // unref this<->desktop
    HashSet_Del(&toplevel_handle->is_inside_desktop, desktop);
    LinkedList_DelElemFromTail(&desktop->toplevel_handles, toplevel_handle);

    if (desktop->selected_handle == toplevel_handle)
        desktop->selected_handle = NULL;

    // determine its desktop to iconify
    if (toplevel_handle->is_inside_desktop.count == 1)
    {
        struct HashSet_Iter iter = { };
        struct Desktop *new_iconify_to_desktop = NULL;

        HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktop);
        new_iconify_to_desktop = HashSet_IterNext(&iter);

        ToplevelHandle_ChangeDesktopToIconify(toplevel_handle, new_iconify_to_desktop);

        ToplevelHandle_DebugPrintfWithTitleAndAppID(toplevel_handle, " will now be iconified to output: %s\n", desktop->output_name);
    }
    else
    {
        ToplevelHandle_DebugPrintDesktops(toplevel_handle);

        if (toplevel_handle->is_inside_desktop.count == 0)
            toplevel_handle->iconify_to_desktop = NULL;
    }
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
    
    // if state minimized changed
    if (minimized_old != toplevel_handle->minimized)
    {
        // change visibility
        toplevel_handle->desktop_entry.visible = toplevel_handle->minimized;

        if (toplevel_handle->minimized)
        {
            toplevel_handle->desktop_entry.has_not_been_minimized_since_last_desktop_change = false;
            
            if (toplevel_handle->desktop_entry.recalc_position_when_minimize)
                toplevel_handle->desktop_entry.recalc_position = true;

            toplevel_handle->last_iconified_to = toplevel_handle->iconify_to_desktop;
        }

        // request redraw
        toplevel_handle->desktop_entry.redraw = true;
    }

    ToplevelHandle_DebugPrintfWithTitleAndAppID(
        toplevel_handle,
        " states:\n"
        "\tmaximized: %d, minimized: %d, activated: %d, fullscreen: %d\n",
        toplevel_handle->maximized, toplevel_handle->minimized, toplevel_handle->activated, toplevel_handle->fullscreen
    );
}

static void ToplevelHandle_Done(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct ToplevelHandle *toplevel_handle = data;

    // currently left empty
}

static void ToplevelHandle_Closed(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle
)
{
    struct ToplevelHandle *toplevel_handle = data;

    toplevel_handle->desktop_entry.redraw  = true;
    toplevel_handle->desktop_entry.visible = false;

    // unref this from toplevel manager
    LinkedList_DelNode(toplevel_handle->linkedlist_node);

    // deconstruct pending
    struct LinkedList_Node *node = LinkedList_CreateNode(toplevel_handle);
    LinkedList_InsertNodeTail(&global_toplevel_handles_closed, node);
    toplevel_handle->linkedlist_node = node;

    ToplevelHandle_DebugPrintfWithTitleAndAppID(toplevel_handle, " has been closed\n");
}

static void ToplevelHandle_Parent(
    void *data,
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel_handle,
    struct zwlr_foreign_toplevel_handle_v1 *parent
)
{
    struct ToplevelHandle *toplevel_handle = data;
    struct ToplevelHandle *toplevel_handle_parent = NULL;

    if (!parent)
        return;

    toplevel_handle_parent = zwlr_foreign_toplevel_handle_v1_get_user_data(parent);

    HashSet_Add(&toplevel_handle_parent->children, toplevel_handle, NULL);

    toplevel_handle->parent = toplevel_handle_parent;

    ToplevelHandle_DebugPrintfWithTitleAndAppID(toplevel_handle, " is child of ");
    ToplevelHandle_DebugPrintfWithTitleAndAppID(toplevel_handle_parent, "\n");
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

    HashSet_Init(&toplevel_handle->children);
    HashSet_Init(&toplevel_handle->is_inside_desktop);

    toplevel_handle->desktop_entry.w = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X;
    toplevel_handle->desktop_entry.h = DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y;

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
    struct HashSet_Iter iter = { };
    struct ToplevelHandle *toplevel_handle_child = NULL;
    struct Desktop *desktop = NULL;

    // free strings
    if (toplevel_handle->title)
        free(toplevel_handle->title);
    if (toplevel_handle->app_id)
        free(toplevel_handle->app_id);

    // unref if parent exist
    if (toplevel_handle->parent)
        HashSet_Del(&toplevel_handle->parent->children, toplevel_handle);
    
    // unref child and cleanup
    HashSet_IterInit(&iter, &toplevel_handle->children);
    while ((toplevel_handle_child = HashSet_IterDel(&iter)))
        toplevel_handle_child->parent = NULL;
    HashSet_Cleanup(&toplevel_handle->children);

    // unref from Output and cleanup
    HashSet_IterInit(&iter, &toplevel_handle->is_inside_desktop);
    while ((desktop = HashSet_IterDel(&iter)))
    {
        // unselect removed toplevel handle
        if (desktop->selected_handle == toplevel_handle)
            desktop->selected_handle = NULL;

        // no need to send front now
        struct LinkedList_Node *toplevel_handle_node = LinkedList_FindNodeFromTail(&desktop->toplevel_handles, toplevel_handle);
        if (desktop->toplevel_handle_to_send_front == toplevel_handle_node)
            desktop->toplevel_handle_to_send_front = NULL;
        LinkedList_DestroyNode(toplevel_handle_node);
    }
    HashSet_Cleanup(&toplevel_handle->is_inside_desktop);

    // remove from toplevel manager
    if (toplevel_handle->linkedlist_node)
        LinkedList_DestroyNode(toplevel_handle->linkedlist_node);

    // destroy zwlr handle
    zwlr_foreign_toplevel_handle_v1_destroy(toplevel_handle->toplevel_handle);

    SWRender_BufferFree(&toplevel_handle->desktop_entry.icon_buf);

    free(toplevel_handle);
}
