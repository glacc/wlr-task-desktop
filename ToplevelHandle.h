#pragma once

#include <stdbool.h>

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "HashSet.h"

#include "SWRender/SWRender.h"

// TODO: make icon size configurable?
#define DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_X        96
#define DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ENTRY_SIZE_Y        112

#define DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_X         48
#define DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_SIZE_Y         48
#define DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TOP_PADDING    4

#define DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TEXT_SPACING   4
#define DEFAULT_TOPLEVEL_HANDLE_DESKTOP_ICON_TEXT_SIZE      8.5F

struct ToplevelHandle_DesktopEntry
{
    int x, y;
    bool has_position;

    // reserved for future use?
    int w, h;

    // usually hides when not minimized, no effect in stable mode
    bool visible;

    // set to true when ToplevelHandle.iconify_to_desktop changes
    bool has_not_been_minimized_since_last_desktop_change;

    // could be disabled by: dragging icon, stable mode
    bool recalc_position_when_minimize;

    bool recalc_position;

    bool redraw;

    SWRenderBuffer icon_buf;
};

struct ToplevelHandle
{
    struct zwlr_foreign_toplevel_handle_v1 *toplevel_handle;

    char *title;
    char *app_id;

    bool maximized, minimized, activated, fullscreen;

    struct ToplevelHandle *parent;
    struct HashSet children;    // typeof(struct ToplevelHandle *)
    
    struct HashSet is_inside_desktops;

    // only updated if there's only one Output in is_inside_outputs. NULL if zero.
    struct Desktop *iconify_to_desktop;

    struct ToplevelHandle_DesktopEntry desktop_entry;
};

extern struct ToplevelHandle *ToplevelHandle_Create(void);
extern void ToplevelHandle_Destroy(struct ToplevelHandle *toplevel_handle);

extern void ToplevelHandle_PrintIsInsideWhichOutput(struct ToplevelHandle *toplevel_handle);

extern const struct zwlr_foreign_toplevel_handle_v1_listener ToplevelHandle_listener;
