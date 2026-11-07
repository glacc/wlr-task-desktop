#pragma once

#include <wayland-client.h>

struct KeyboardState
{

};

struct Keyboard
{
    struct wl_keyboard *wl_keyboard;

    struct wl_surface *wl_surface;

    
};

extern void Keyboard_GetWlObj(struct Keyboard *keyboard, struct wl_seat *seat);
extern void Keyboard_DestroyWlObj(struct Keyboard *keyboard);

extern const struct wl_keyboard_listener Keyboard_listener;
