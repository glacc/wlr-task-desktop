#include "Pointer.h"

#include <stdio.h>
#include <malloc.h>

#include "DoubleBufferedSurface.h"

#include "Utility.h"

int Pointer_debug_verbosity = 1;

static void Pointer_SetBtnStateInternal(struct PointerState *pointer_state, uint32_t button, bool down, uint32_t time)
{
    struct PointerBtnState *btn_state = NULL;

    if (!(btn_state = Pointer_GetBtnState(pointer_state, button)))
        return;

    btn_state->pressed = down;
    btn_state->release = !down;
    btn_state->is_down = down;

    if (down)
    {
        // double click check
        uint32_t time_between_last_click = time - btn_state->pressed_time;

        if (time_between_last_click < POINTER_DOUBLE_CLICK_TIME_MS)
        {
            wl_fixed_t dist_sq_between_presses = Utility_DistSqBetweenVecWlFixed(
                btn_state->pressed_x,
                btn_state->pressed_y,
                pointer_state->x,
                pointer_state->y
            );

            // not double click if distance too far between two presses
            if (dist_sq_between_presses < POINTER_DOUBLE_CLICK_DIST_THRESHOLD_SQ_WL_FIXED)
            {
                btn_state->double_click = true;
            
                btn_state->double_click_x = pointer_state->x;
                btn_state->double_click_x = pointer_state->y;
                btn_state->double_click_x_int = pointer_state->x_int;
                btn_state->double_click_x_int = pointer_state->y_int;
                btn_state->double_click_time = time;
            }
        }

        // set press state
        btn_state->pressed_x = pointer_state->x;
        btn_state->pressed_y = pointer_state->y;
        btn_state->pressed_x_int = pointer_state->x_int;
        btn_state->pressed_y_int = pointer_state->y_int;
        btn_state->pressed_time = time;
    }
    else
    {
        btn_state->release_x = pointer_state->x;
        btn_state->release_y = pointer_state->y;
        btn_state->release_x_int = pointer_state->x_int;
        btn_state->release_y_int = pointer_state->y_int;
        btn_state->release_time = time;

        btn_state->dragging = false;
        
        if (btn_state->dragging)
        {
            btn_state->drag_stop = true;
            btn_state->drag_stop_time = time;
        }

    }
}

#pragma region WaylandCallbacks

static void Pointer_Enter(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *wl_surface,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct Pointer *pointer = data;
    struct DoubleBufferedSurface *surface = wl_surface_get_user_data(wl_surface);

    pointer->surface = surface;

    if (surface)
    {
        HashSet_Add(&pointer->surfaces_to_manage, surface, NULL);
        surface->pointer = pointer;
        surface->pointer_state.pointer_inside = true;

        if (Pointer_debug_verbosity >= 2)
            printf("pointer entered surface %s\n", surface->name);
    }
}

static void Pointer_Leave(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    struct wl_surface *wl_surface
)
{
    struct Pointer *pointer = data;
    
    if (!wl_surface)
        return;

    struct DoubleBufferedSurface *surface = wl_surface_get_user_data(wl_surface);

    if (pointer->surface == wl_surface_get_user_data(wl_surface))
        pointer->surface = NULL;

    if (surface)
    {
        surface->pointer_state.pointer_inside = false;

        if (Pointer_debug_verbosity >= 2)
            printf("pointer left surface %s\n", surface->name);
    }
}

static void Pointer_Motion(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    struct Pointer *pointer = data;
    
    struct DoubleBufferedSurface *surface = pointer->surface;

    if (!surface)
        return;

    int surface_x_int = Utility_WlFixedToInt(surface_x);
    int surface_y_int = Utility_WlFixedToInt(surface_y);

    struct PointerState *pointer_state = &surface->pointer_state;

    pointer_state->motion_time = time;
    pointer_state->x = surface_x;
    pointer_state->y = surface_y;
    pointer_state->x_int = surface_x_int;
    pointer_state->y_int = surface_y_int;

    struct HashSet_Iter iter = { };
    if (HashSet_IterInit(&iter, &pointer_state->button_states))
    {
        struct PointerBtnState *btn_state = NULL;

        while ((btn_state = HashSet_IterNext(&iter)))
        {
            if (!btn_state->is_down)
                continue;

            if (btn_state->dragging)
                continue;

            wl_fixed_t dist_sq_from_press = Utility_DistSqBetweenVecWlFixed(
                surface_x,
                surface_y,
                btn_state->pressed_x,
                btn_state->pressed_y
            );

            if (dist_sq_from_press > POINTER_DRAG_START_THRESHOLD_SQ_WL_FIXED)
            {
                btn_state->drag_start = true;
                btn_state->dragging = true;

                btn_state->drag_start_x = surface_x;
                btn_state->drag_start_y = surface_y;
                btn_state->drag_start_x_int = surface_x_int;
                btn_state->drag_start_y_int = surface_y_int;

                btn_state->drag_start_time = time;

                if (Pointer_debug_verbosity >= 1)
                {
                    printf(
                        "pointer button %u starts dragging from (%d, %d) in %s\n",
                        btn_state->button, surface_x, surface_y, surface->name
                    );
                }
            }
        }
    }
}

static void Pointer_Button(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t serial,
    uint32_t time,
    uint32_t button,
    uint32_t state
)
{
    struct Pointer *pointer = data;
    const char *button_state_str[2] =
    {
        "released",
        "pressed",
    };
    
    struct DoubleBufferedSurface *surface = pointer->surface;

    if (!surface)
        return;

    struct PointerState *pointer_state = &surface->pointer_state;

    Pointer_SetBtnStateInternal(pointer_state, button, (state == WL_POINTER_BUTTON_STATE_PRESSED), time);

    if (Pointer_debug_verbosity < 1)
        return;
    
    printf(
        "button %u %s in %s (%d, %d)\n",
        button,
        button_state_str[state],
        surface->name,
        (pointer_state->x + 128) >> 8,
        (pointer_state->y + 128) >> 8
    );
}

#pragma region WaylandPointerAxisCallbacks

static void Pointer_Axis(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis,
    wl_fixed_t value
)
{

}

static void Pointer_AxisSource(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis_source
)
{

}

static void Pointer_AxisStop(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t time,
    uint32_t axis
)
{

}

static void Pointer_AxisDiscrete(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    int32_t discrete
)
{

}

static void Pointer_AxisValue120(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    int32_t value120
)
{

}

static void Pointer_AxisRelativeDirection(
    void *data,
    struct wl_pointer *wl_pointer,
    uint32_t axis,
    uint32_t direction
)
{

}

#pragma endregion

static void Pointer_Warp(
    void *data,
    struct wl_pointer *wl_pointer,
    wl_fixed_t surface_x,
    wl_fixed_t surface_y
)
{
    /*
    struct Pointer *pointer = data;
    
    struct DoubleBufferedSurface *surface = pointer->surface;

    if (!surface)
        return;

    struct PointerState *pointer_state = &surface->pointer_state;

    // necessary?
    pointer_state->x = surface_x;
    pointer_state->y = surface_y;
    */
}

static void Pointer_Frame(
    void *data,
    struct wl_pointer *wl_pointer
)
{
    // pointer event frame ends
}

const struct wl_pointer_listener Pointer_listener =
{
    .enter = Pointer_Enter,
    .leave = Pointer_Leave,
    .motion = Pointer_Motion,
    .button = Pointer_Button,
    .axis = Pointer_Axis,
    .frame = Pointer_Frame,
    .axis_source = Pointer_AxisSource,
    .axis_stop = Pointer_AxisStop,
    .axis_discrete = Pointer_AxisDiscrete,
    .axis_value120 = Pointer_AxisValue120,
    .axis_relative_direction = Pointer_AxisRelativeDirection,
    .warp = Pointer_Warp,
};

#pragma endregion

#pragma region PointerBtnState

struct PointerBtnState *PointerBtnState_Create(uint32_t button)
{
    struct PointerBtnState *btn_state = calloc(1, sizeof(*btn_state));

    if (!btn_state)
        return NULL;

    btn_state->button = button;

    return btn_state;
}

void PointerBtnState_Destroy(struct PointerBtnState* btn_state)
{
    if (btn_state)
        free(btn_state);
}

size_t PointerBtnState_Hash(void *btn_state)
{
    return ((struct PointerBtnState *)btn_state)->button;
}

bool PointerBtnState_IsEqual(void *l, void *r)
{
    struct PointerBtnState *btn_state_l = (struct PointerBtnState *)l;
    struct PointerBtnState *btn_state_r = (struct PointerBtnState *)r;
    return (btn_state_l->button == btn_state_r->button);
}

#pragma endregion

#pragma region PointerState

bool PointerState_Init(struct PointerState *pointer_state)
{
    if (!pointer_state)
        return false;

    pointer_state->x_old = pointer_state->x = 0;
    pointer_state->y_old = pointer_state->y = 0;
    pointer_state->pointer_inside = false;

    if (!HashSet_Init(&pointer_state->button_states))
        return false;
    pointer_state->button_states.Hash = PointerBtnState_Hash;
    pointer_state->button_states.IsEqual = PointerBtnState_IsEqual;

    return true;
}

void PointerState_Cleanup(struct PointerState *pointer_state)
{
    if (!pointer_state)
        return;

    struct HashSet_Iter iter = { };
    if (HashSet_IterInit(&iter, &pointer_state->button_states))
    {
        struct PointerBtnState *btn_state = NULL;

        while ((btn_state = HashSet_IterDel(&iter)))
            PointerBtnState_Destroy(btn_state);
    }
    HashSet_Cleanup(&pointer_state->button_states);
}

void PointerState_PostDispatch(struct PointerState *pointer_state)
{
    if (!pointer_state)
        return;

    pointer_state->x_old = pointer_state->x;
    pointer_state->y_old = pointer_state->y;
    pointer_state->x_old_int = pointer_state->x_int;
    pointer_state->y_old_int = pointer_state->y_int;
    pointer_state->motion_old_time = pointer_state->motion_time;

    struct HashSet_Iter iter = { };
    if (HashSet_IterInit(&iter, &pointer_state->button_states))
    {
        struct PointerBtnState *btn_state = NULL;

        while ((btn_state = HashSet_IterNext(&iter)))
        {
            btn_state->pressed = false;
            btn_state->release = false;
            btn_state->double_click = false;

            btn_state->drag_start = false;
            btn_state->drag_stop = false;
        }
    }
}

#pragma endregion

bool Pointer_Init(struct Pointer *pointer)
{
    if (!pointer)
        return false;

    if (!HashSet_Init(&pointer->surfaces_to_manage))
        return false;

    return true;
}

void Pointer_Cleanup(struct Pointer *pointer)
{
    if (!pointer)
        return;

    // unref
    // can i avoid this kind of bidirection ref?
    struct HashSet_Iter iter = { };
    if (HashSet_IterInit(&iter, &pointer->surfaces_to_manage))
    {
        struct DoubleBufferedSurface *surface = NULL;

        while ((surface = HashSet_IterDel(&iter)))
        {
            if (surface->pointer == pointer)
                surface->pointer = NULL;
        }
    }
    HashSet_Cleanup(&pointer->surfaces_to_manage);

    if (pointer->wl_pointer)
    {
        wl_pointer_destroy(pointer->wl_pointer);
        pointer->wl_pointer = NULL;
    }
}

void Pointer_GetWlObj(struct Pointer *pointer, struct wl_seat *seat)
{
    if (!pointer)
        return;

    pointer->wl_pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(pointer->wl_pointer, &Pointer_listener, pointer);
}

void Pointer_DestroyWlObj(struct Pointer *pointer)
{
    if (!pointer)
        return;

    if (!pointer->wl_pointer)
        return;

    wl_pointer_destroy(pointer->wl_pointer);
    pointer->wl_pointer = NULL;
}

struct PointerBtnState *Pointer_GetBtnState(struct PointerState *pointer_state, uint32_t button)
{
    if (!pointer_state)
        return NULL;

    struct PointerBtnState btn_state_to_find = { .button = button };
    struct PointerBtnState *btn_state = NULL;

    if ((btn_state = HashSet_Find(&pointer_state->button_states, &btn_state_to_find)))
        return btn_state;

    if (!(btn_state = PointerBtnState_Create(button)))
        return NULL;
    
    if (!HashSet_Add(&pointer_state->button_states, btn_state, NULL))
    {
        PointerBtnState_Destroy(btn_state);
        return NULL;
    }

    return btn_state;
}
