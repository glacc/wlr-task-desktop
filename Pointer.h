#pragma once

#include <wayland-client.h>

#include "HashSet.h"

#define POINTER_DRAG_START_THRESHOLD_1X 5
#define POINTER_DRAG_START_THRESHOLD_SQ_WL_FIXED ((POINTER_DRAG_START_THRESHOLD_1X * POINTER_DRAG_START_THRESHOLD_1X) << 8)

struct PointerBtnState
{
    uint32_t button;
    
    bool is_down;

    bool pressed;
    wl_fixed_t pressed_x;
    wl_fixed_t pressed_y;

    bool release;
    wl_fixed_t release_x;
    wl_fixed_t release_y;

    bool drag_start;
    bool dragging;
    bool drag_stop;
    wl_fixed_t drag_start_x;
    wl_fixed_t drag_start_y;

    uint32_t pressed_time;
    uint32_t release_time;
    uint32_t drag_start_time;
    uint32_t drag_stop_time;
};

struct PointerState
{
    wl_fixed_t x;
    wl_fixed_t y;
    uint32_t motion_time;
    wl_fixed_t x_old;
    wl_fixed_t y_old;
    uint32_t motion_time_old;

    bool pointer_inside;

    // typeof(struct PointerBtnState *)
    struct HashSet button_states;
};

struct Pointer
{
    struct wl_pointer *wl_pointer;

    struct DoubleBufferedSurface *surface;

    // typeof(struct DoubleBufferedSurface *)
    struct HashSet surfaces_to_manage;
};

extern struct PointerBtnState *PointerBtnState_Create(uint32_t button);
extern void PointerBtnState_Destroy(struct PointerBtnState* btn_state);

extern size_t PointerBtnState_Hash(void *btn_state);
extern bool PointerBtnState_IsEqual(void *l, void *r);

extern bool PointerState_Init(struct PointerState *pointer_state);
extern void PointerState_Cleanup(struct PointerState *pointer_state);

extern void PointerState_PostDispatch(struct PointerState *pointer_state);

extern bool Pointer_Init(struct Pointer *pointer);
extern void Pointer_Cleanup(struct Pointer *pointer);

extern void Pointer_GetWlObj(struct Pointer *pointer, struct wl_seat *seat);
extern void Pointer_DestroyWlObj(struct Pointer *pointer);

extern struct PointerBtnState *Pointer_GetBtnState(struct PointerState *pointer_state, uint32_t button);

extern const struct wl_pointer_listener Pointer_wl_listener;
