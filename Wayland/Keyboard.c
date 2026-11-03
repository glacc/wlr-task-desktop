#include "Keyboard.h"

#include <stdio.h>

void Keyboard_GetWlObj(struct Keyboard *keyboard, struct wl_seat *seat)
{
    if (!keyboard)
        return;

    keyboard->wl_keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(keyboard->wl_keyboard, &Keyboard_wayland_listener, keyboard);
}

void Keyboard_DestroyWlObj(struct Keyboard *keyboard)
{
    if (!keyboard)
        return;

    if (!keyboard->wl_keyboard)
        return;

    wl_keyboard_destroy(keyboard->wl_keyboard);
    keyboard->wl_keyboard = NULL;
}

static void Keyboard_Keymap(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t format,
    int32_t fd,
    uint32_t size
)
{
    printf(
        "keymap from fd %d in %u bytes with format %u\n",
        fd, size, format
    );
}

static void Keyboard_Enter(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    struct wl_surface *surface,
    struct wl_array *keys
)
{
    printf("keyboard focused\n");
}

static void Keyboard_Leave(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    struct wl_surface *surface
)
{
    printf("keyboard unfocused\n");
}

static void Keyboard_Key(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    uint32_t time,
    uint32_t key,
    uint32_t state
)
{
    const char *key_state_str[3] =
    {
        "released",
        "pressed",
        "repeated",
    };
    printf("keyboard key %u %s\n", key, key_state_str[state]);
}

static void Keyboard_Modifiers(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    uint32_t mods_depressed,
    uint32_t mods_latched,
    uint32_t mods_locked,
    uint32_t group
)
{

}

static void Keyboard_RepeatInfo(
    void *data,
    struct wl_keyboard *wl_keyboard,
    int32_t rate,
    int32_t delay
)
{

}

const struct wl_keyboard_listener Keyboard_wayland_listener =
{
    .keymap = Keyboard_Keymap,
    .enter = Keyboard_Enter,
    .leave = Keyboard_Leave,
    .key = Keyboard_Key,
    .modifiers = Keyboard_Modifiers,
    .repeat_info = Keyboard_RepeatInfo,
};
