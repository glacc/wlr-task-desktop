#pragma once

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "HashSet.h"

struct ToplevelManager
{
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager;

    // typeof(struct ToplevelHandle *)
    struct HashSet handles;
};

extern bool ToplevelManager_Init(struct ToplevelManager *toplevel_manager);
extern void ToplevelManager_Cleanup(struct ToplevelManager *toplevel_manager);

extern const struct zwlr_foreign_toplevel_manager_v1_listener ToplevelManager_wl_listener;
