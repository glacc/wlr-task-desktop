#pragma once

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "HashSet.h"

struct ToplevelHandle
{
    struct zwlr_foreign_toplevel_handle_v1 *toplevel_handle;

    struct AppState *app_state;

    char *title;
    char *app_id;

    bool maximized, minimized, activated, fullscreen;

    struct ToplevelHandle *parent;
    struct HashSet child;    // typeof(struct ToplevelHandle *)
    
    struct HashSet is_inside_desktops;

    // only updated if there's only one Output in is_inside_outputs. NULL if zero.
    struct Desktop *iconify_to_desktop;
};

extern struct ToplevelHandle *ToplevelHandle_Create(void);
extern void ToplevelHandle_Destroy(struct ToplevelHandle *toplevel_handle);

extern void ToplevelHandle_PrintIsInsideWhichOutput(struct ToplevelHandle *toplevel_handle);

extern const struct zwlr_foreign_toplevel_handle_v1_listener ToplevelHandle_wl_listener;
