#pragma once

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "HashSet.h"

#include "ToplevelManager.h"

struct ToplevelHandle
{
    struct zwlr_foreign_toplevel_handle_v1 *toplevel_handle;
    
    struct ToplevelManager *manager;

    char *title;
    char *app_id;

    bool maximized, minimized, activated, fullscreen;

    struct ToplevelHandle *parent;
    struct HashSet child;    // typeof(struct ToplevelHandle *)
    
    struct HashSet is_inside_outputs;
};

extern struct ToplevelHandle *ToplevelHandle_New(void);
extern void ToplevelHandle_Destroy(struct ToplevelHandle *toplevel_handle);

extern void ToplevelHandle_PrintIsInsideWhichOutput(struct ToplevelHandle *toplevel_handle);

extern const struct zwlr_foreign_toplevel_handle_v1_listener ToplevelHandle_wl_listener;
