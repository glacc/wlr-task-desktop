#pragma once

#include <signal.h>

#include "LinkedList.h"
#include "HashSet.h"

#include <wayland-client.h>
#include "viewporter-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "Keyboard.h"
#include "Pointer.h"

#include <SWRender/FTEssentials.h>
#include <SWRender/FTWordWrap.h>

extern struct wl_display    *wl_display;
extern struct wl_registry   *wl_registry;

extern struct wl_seat       *wl_seat;
extern struct wp_cursor_shape_manager_v1 *wp_cursor_shape_manager;

extern struct wl_compositor *wl_compositor;
extern struct wl_shm        *wl_shm;
extern struct wp_viewporter *wp_viewporter;

extern struct zwlr_layer_shell_v1 *zwlr_layer_shell;
extern struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager;

extern struct Keyboard global_keyboard;
extern struct Pointer  global_pointer;

extern struct HashSet    global_desktops;          // typeof(struct Desktop *)
extern struct LinkedList global_desktops_created;  // typeof(struct Desktop *)

extern struct LinkedList global_toplevel_handles;          // typeof(struct ToplevelHandle *)
extern struct LinkedList global_toplevel_handles_closed;   // typeof(struct ToplevelHandle *)

extern FTEssentials_State   global_ft_state;
extern FTWordWrapLineBuffer global_ft_line_buffer;

extern volatile sig_atomic_t global_interrupted;
