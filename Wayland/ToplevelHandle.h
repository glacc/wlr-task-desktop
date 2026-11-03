#pragma once

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "ToplevelManager.h"
#include "Output.h"

struct ToplevelHandle
{
    struct zwlr_foreign_toplevel_handle_v1 *toplevel_handle;
    
    struct ToplevelManager *manager;

    char *title;
    char *app_id;

    bool maximized, minimized, activated, fullscreen;

    struct ToplevelHandle *parent;
    struct Vector child;    // typeof(struct ToplevelHandle *)
    
    struct Wayland_Output *output;
};

extern void ToplevelHandle_Cleanup(struct ToplevelHandle *toplevel_handle);

extern const struct zwlr_foreign_toplevel_handle_v1_listener ToplevelHandle_wl_listener;
