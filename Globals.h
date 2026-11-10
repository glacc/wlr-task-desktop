#pragma once

#include <signal.h>

#include <wayland-client.h>
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#include "Keyboard.h"
#include "Pointer.h"

#include "HashSet.h"
#include "Vector.h"

#include "SWRender/FTEssentials.h"
#include "SWRender/FTWordWrap.h"

#ifndef DEBUG_DAMAGE
#define DEBUG_DAMAGE 0
#endif

extern struct wl_display    *wl_display;
extern struct wl_registry   *wl_registry;

extern struct wl_seat       *wl_seat;

extern struct wl_compositor *wl_compositor;
extern struct wl_shm        *wl_shm;
extern struct wp_viewporter *wp_viewporter;

extern struct zwlr_layer_shell_v1 *zwlr_layer_shell;
extern struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager;

extern struct Keyboard global_keyboard;
extern struct Pointer  global_pointer;

extern struct HashSet global_desktops;                              // typeof(struct Desktop *)
extern struct HashSet global_toplevel_handles;                      // typeof(struct ToplevelHandle *)
extern struct Vector global_toplevel_handle_to_destroy;             // typeof(struct ToplevelHandle *)

extern struct Vector global_desktop_with_surface_creation_pending;  // typeof(struct Desktop *)

extern FTEssentials_State global_ft_state;
extern FTWordWrapLineBuffer global_ft_line_buffer;

extern volatile sig_atomic_t global_exit;
