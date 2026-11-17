/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#include "keyboard.h"

#include <stdio.h>
#include <assert.h>
#include <malloc.h>

#include <unistd.h>

#include <sys/mman.h>

#include "main.h"
#include "seat.h"
#include "double_buffer_surface.h"

#include "debug.h"

#ifndef _KEYBOARD_DEBUG_
#define _KEYBOARD_DEBUG_ 0
#endif

static void keyboard_debug_print_keysym(xkb_keysym_t keysym)
{
    char buffer[256];
    xkb_keysym_get_name(keysym, buffer, sizeof(buffer));
    debug_printf("%s", buffer);
}

#pragma region keyboard_key_state

#pragma region keyboard_key_state_hashset_functions

static size_t keyboard_key_state_hash_function(void *ptr)
{
    struct keyboard_key_state *keyboard_key_state = ptr;
    return keyboard_key_state->keysym;
}

static bool keyboard_key_state_is_equal(void *l, void *r)
{
    struct keyboard_key_state *keyboard_key_state_l = l;
    struct keyboard_key_state *keyboard_key_state_r = r;
    return (keyboard_key_state_l->keysym == keyboard_key_state_r->keysym);
}

#pragma endregion

static struct keyboard_key_state *keyboard_key_state_create(xkb_keysym_t keysym)
{
    struct keyboard_key_state *keyboard_key_state = calloc(1, sizeof(*keyboard_key_state));
    keyboard_key_state->keysym = keysym;
    return keyboard_key_state;
}

static void keyboard_key_state_destroy(struct keyboard_key_state *keyboard_key_state)
{
    free(keyboard_key_state);
}

#pragma region keyboard_key_state_actions

static void keyboard_key_state_down(struct keyboard_key_state *keyboard_key_state)
{
    keyboard_key_state->is_down = true;
}

static void keyboard_key_state_press(struct keyboard_key_state *keyboard_key_state, uint32_t time)
{
    if (keyboard_key_state->is_down)
        return;
    
    keyboard_key_state->pressed = true;
    keyboard_key_state->press_time = time;

    keyboard_key_state->is_down = true;
}

static void keyboard_key_state_release(struct keyboard_key_state *keyboard_key_state, uint32_t time)
{
    if (!keyboard_key_state->is_down)
        return;

    keyboard_key_state->is_down = false;
    
    keyboard_key_state->released = true;
    keyboard_key_state->release_time = time;
}

static void keyboard_key_state_repeat(struct keyboard_key_state *keyboard_key_state, uint32_t time)
{
    keyboard_key_state->repeated = true;
    keyboard_key_state->repeat_time = time;
}

#pragma endregion

static void keyboard_key_state_manage_post_update(struct keyboard_key_state *keyboard_key_state)
{
    keyboard_key_state->pressed  = false;
    keyboard_key_state->released = false;
    keyboard_key_state->repeated = false;
}

#pragma endregion

#pragma region keyboard_state

#pragma region keyboard_state_hashset_functions

size_t keyboard_state_hash_function(void *ptr)
{
    struct keyboard_state *keyboard_state = ptr;
    return hashset_default_hash_function(keyboard_state->seat);
}

bool keyboard_state_is_equal(void *l, void *r)
{
    struct keyboard_state *keyboard_state_l = l;
    struct keyboard_state *keyboard_state_r = r;
    return (keyboard_state_l->seat == keyboard_state_r->seat);
}

#pragma endregion

void keyboard_state_init(struct keyboard_state *keyboard_state, struct seat *seat)
{
    assert(keyboard_state != NULL && seat != NULL);

    keyboard_state->seat = seat;

    hashset_init(&keyboard_state->key_states);
    keyboard_state->key_states.hash_function = keyboard_key_state_hash_function;
    keyboard_state->key_states.is_equal = keyboard_key_state_is_equal;

    vector_init(&keyboard_state->keys_pressed, sizeof(struct keyboard_key_state *));
    vector_init(&keyboard_state->keys_released, sizeof(struct keyboard_key_state *));
    vector_init(&keyboard_state->keys_repeated, sizeof(struct keyboard_key_state *));
}

void keyboard_state_cleanup(struct keyboard_state *keyboard_state)
{
    assert(keyboard_state != NULL);

    vector_cleanup(&keyboard_state->keys_pressed);
    vector_cleanup(&keyboard_state->keys_released);
    vector_cleanup(&keyboard_state->keys_repeated);

    struct hashset_iterator iterator_key_states = { };
    struct keyboard_key_state *key_state = { };
    hashset_iterator_init(&iterator_key_states, &keyboard_state->key_states);
    while ((key_state = hashset_iterator_next_deletion(&iterator_key_states)))
        keyboard_key_state_destroy(key_state);

    hashset_cleanup(&keyboard_state->key_states);
}

struct keyboard_state *keyboard_state_create(struct seat *seat)
{
    assert(seat != NULL);

    struct keyboard_state *keyboard_state = calloc(1, sizeof(*keyboard_state));
    assert(keyboard_state != NULL);

    keyboard_state_init(keyboard_state, seat);

    return keyboard_state;
}

void keyboard_state_destroy(struct keyboard_state *keyboard_state)
{
    assert(keyboard_state != NULL);

    keyboard_state_cleanup(keyboard_state);

    free(keyboard_state);
}

struct keyboard_key_state *keyboard_state_get_key_state(struct keyboard_state *keyboard_state, xkb_keysym_t keysym)
{
    assert(keyboard_state != NULL);

    struct keyboard_key_state keyboard_key_state_to_find = { .keysym = keysym };
    struct keyboard_key_state *keyboard_key_state = hashset_find(&keyboard_state->key_states, &keyboard_key_state_to_find);
    if (keyboard_key_state)
        return keyboard_key_state;

    keyboard_key_state = keyboard_key_state_create(keysym);

    struct keyboard_key_state *overwritten_keyboard_key_state = NULL;
    hashset_insert(&keyboard_state->key_states, keyboard_key_state, (void **)&overwritten_keyboard_key_state);
    assert(overwritten_keyboard_key_state == NULL);

    return keyboard_key_state;
}

#pragma region keyboard_state_actions

static void keyboard_state_press(struct keyboard_state *keyboard_state, xkb_keysym_t keysym, uint32_t time)
{
    struct keyboard_key_state *keyboard_key_state = keyboard_state_get_key_state(keyboard_state, keysym);

#if _KEYBOARD_DEBUG_ == 1
    if (!keyboard_key_state->is_down)
    {
        debug_printf("seat '%s' keyboard key '", debug_str_nullable(keyboard_state->seat->identifier));
        keyboard_debug_print_keysym(keysym);
        debug_printf("' pressed\n");
    }
#endif

    keyboard_key_state_press(keyboard_key_state, time);
    vector_insert(&keyboard_state->keys_pressed, &keyboard_key_state);
}

static void keyboard_state_release(struct keyboard_state *keyboard_state, xkb_keysym_t keysym, uint32_t time)
{
    struct keyboard_key_state *keyboard_key_state = keyboard_state_get_key_state(keyboard_state, keysym);

#if _KEYBOARD_DEBUG_ == 1
    if (keyboard_key_state->is_down)
    {
        debug_printf("seat '%s' keyboard key '", debug_str_nullable(keyboard_state->seat->identifier));
        keyboard_debug_print_keysym(keysym);
        debug_printf("' released\n");
    }
#endif

    keyboard_key_state_release(keyboard_key_state, time);
    vector_insert(&keyboard_state->keys_released, &keyboard_key_state);
}

static void keyboard_state_repeat(struct keyboard_state *keyboard_state, xkb_keysym_t keysym, uint32_t time)
{
    struct keyboard_key_state *keyboard_key_state = keyboard_state_get_key_state(keyboard_state, keysym);
    keyboard_key_state_repeat(keyboard_key_state, time);
    vector_insert(&keyboard_state->keys_repeated, &keyboard_key_state);

#if _KEYBOARD_DEBUG_ == 1
    debug_printf("seat '%s' keyboard key '", debug_str_nullable(keyboard_state->seat->identifier));
    keyboard_debug_print_keysym(keysym);
    debug_printf("' repeated\n");
#endif
}

static void keyboard_state_enter(struct keyboard_state *keyboard_state, struct wl_array *keys)
{
    struct xkb_state *xkb_state = keyboard_state->seat->keyboard.xkb_state;

#if _KEYBOARD_DEBUG_ == 1
    debug_printf("seat '%s' keyboard entered surface", debug_str_nullable(keyboard_state->seat->identifier));
    bool first_key = true;
#endif

    uint32_t *key;
    wl_array_for_each(key, keys)
    {
        xkb_keycode_t xkb_keycode = *key + 8;
        xkb_keysym_t  xkb_keysym = xkb_state_key_get_one_sym(xkb_state, xkb_keycode);

        struct keyboard_key_state *keyboard_key_state = keyboard_state_get_key_state(keyboard_state, xkb_keysym);
        keyboard_key_state_down(keyboard_key_state);

    #if _KEYBOARD_DEBUG_ == 1
        if (first_key)
            debug_printf(" with key(s) down: ");
        keyboard_debug_print_keysym(xkb_keysym);
        debug_printf(", ");
        first_key = false;
    #endif
    }

#if _KEYBOARD_DEBUG_ == 1
    debug_printf("\n");
#endif
}

static void keyboard_state_leave(struct keyboard_state *keyboard_state)
{
#if _KEYBOARD_DEBUG_ == 1
    debug_printf("seat '%s' keyboard leaved surface", debug_str_nullable(keyboard_state->seat->identifier));
    bool first_key = true;
#endif

    struct hashset_iterator iterator_key_states = { };
    struct keyboard_key_state *key_state = NULL;
    hashset_iterator_init(&iterator_key_states, &keyboard_state->key_states);
    while ((key_state = hashset_iterator_next(&iterator_key_states)))
    {
        if (key_state->is_down)
        {
            vector_insert(&keyboard_state->keys_released, &key_state);

        #if _KEYBOARD_DEBUG_ == 1
            if (first_key)
                debug_printf(" with key(s) released: ");
            keyboard_debug_print_keysym(key_state->keysym);
            debug_printf(", ");
            first_key = false;
        #endif
        }

        // TODO: is release_time needed?
        keyboard_key_state_release(key_state, key_state->release_time);
    }
    
#if _KEYBOARD_DEBUG_ == 1
    debug_printf("\n");
#endif
}

#pragma endregion

static void keyboard_state_manage_post_update(struct keyboard_state *keyboard_state)
{
    struct hashset_iterator iterator_key_states = { };
    struct keyboard_key_state *key_state = NULL;
    hashset_iterator_init(&iterator_key_states, &keyboard_state->key_states);
    while ((key_state = hashset_iterator_next(&iterator_key_states)))
        keyboard_key_state_manage_post_update(key_state);

    vector_clear(&keyboard_state->keys_pressed);
    vector_clear(&keyboard_state->keys_released);
    vector_clear(&keyboard_state->keys_repeated);
}

#pragma endregion

#pragma region keyboard_listeners

void keyboard_keymap(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t format,
    int32_t fd,
    uint32_t size
)
{
    struct keyboard *keyboard = data;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    {
        fprintf(stderr, "seat '%s' keyboard received non xkb key map\n", debug_str_nullable(keyboard->seat->identifier));
        close(fd);

        // create fallback xkb_keymap
        if (!keyboard->xkb_keymap)
        {
            keyboard->xkb_keymap = xkb_keymap_new_from_names(xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

            if (!keyboard->xkb_keymap)
                goto err_fallback_keymap;
        }

        if (!keyboard->xkb_state)
        {
            keyboard->xkb_state = xkb_state_new(keyboard->xkb_keymap);

            if (!keyboard->xkb_state)
            {
                xkb_keymap_unref(keyboard->xkb_keymap);
                keyboard->xkb_keymap = NULL;

                goto err_fallback_keymap;
            }
        }

        return;

    err_fallback_keymap:
        fprintf(stderr, "seat '%s' failed to create fallback keymap\n", debug_str_nullable(keyboard->seat->identifier));

        return;
    }

    char *keymap_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(keymap_shm != MAP_FAILED);

    struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_buffer(xkb_context, keymap_shm, size, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (xkb_keymap == NULL)
        goto end;

    struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
    if (xkb_state == NULL)
    {
        xkb_keymap_unref(xkb_keymap);
        goto end;
    }

    if (keyboard->xkb_state)
        xkb_state_unref(keyboard->xkb_state);
    if (keyboard->xkb_keymap)
        xkb_keymap_unref(keyboard->xkb_keymap);

    keyboard->xkb_keymap = xkb_keymap;
    keyboard->xkb_state  = xkb_state;

    keyboard->has_xkb_keymap = true;

    debug_printf("seat '%s' keyboard keymap from fd %d in %u bytes loaded in format %u\n", debug_str_nullable(keyboard->seat->identifier), fd, size, format);

end:
    munmap(keymap_shm, size);
    close(fd);

    return;
}

void keyboard_enter(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    struct wl_surface *wl_surface,
    struct wl_array *keys
)
{
    struct keyboard *keyboard = data;

    if (!wl_surface)
        return;

    keyboard->surface_curr = wl_surface_get_user_data(wl_surface);
    
    struct keyboard_state *keyboard_state = double_buffer_surface_create_or_get_keyboard_state(keyboard->surface_curr, keyboard->seat);
    keyboard_state->keyboard_enter_serial = serial;
    keyboard_state->keyboard_focused = true;

    if (!keyboard->has_xkb_keymap)
        return;

    hashset_insert(&keyboard->surface_visited, keyboard->surface_curr, NULL);

    keyboard_state_enter(keyboard_state, keys);
}

void keyboard_leave(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    struct wl_surface *wl_surface
)
{
    struct keyboard *keyboard = data;

    if (!keyboard->surface_curr)
        return;

    struct keyboard_state *keyboard_state = double_buffer_surface_create_or_get_keyboard_state(keyboard->surface_curr, keyboard->seat);
    keyboard_state->keyboard_leave_serial = serial;
    keyboard_state->keyboard_focused = false;
    keyboard_state_leave(keyboard_state);

    keyboard->surface_curr = NULL;
}

void keyboard_key(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    uint32_t time,
    uint32_t key,
    uint32_t state
)
{
    struct keyboard *keyboard = data;

    if (!keyboard->has_xkb_keymap)
        return;

    if (!keyboard->surface_curr)
        return;

    xkb_keycode_t xkb_keycode = key + 8;
    xkb_keysym_t  xkb_keysym = xkb_state_key_get_one_sym(keyboard->xkb_state, xkb_keycode);

    struct keyboard_state *keyboard_state = double_buffer_surface_create_or_get_keyboard_state(keyboard->surface_curr, keyboard->seat);

    switch (state)
    {
        case WL_KEYBOARD_KEY_STATE_PRESSED:
            keyboard_state_press(keyboard_state, xkb_keysym, time);
            break;
        case WL_KEYBOARD_KEY_STATE_RELEASED:
            keyboard_state_release(keyboard_state, xkb_keysym, time);
            break;
        case WL_KEYBOARD_KEY_STATE_REPEATED:
            keyboard_state_repeat(keyboard_state, xkb_keysym, time);
            break;
    }
}

void keyboard_modifiers(
    void *data,
    struct wl_keyboard *wl_keyboard,
    uint32_t serial,
    uint32_t mods_depressed,
    uint32_t mods_latched,
    uint32_t mods_locked,
    uint32_t group
)
{
    struct keyboard *keyboard = data;

    if (!keyboard->has_xkb_keymap)
        return;

    if (!keyboard->surface_curr)
        return;

    xkb_state_update_mask(keyboard->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void keyboard_repeat_info(
    void *data,
    struct wl_keyboard *wl_keyboard,
    int32_t rate,
    int32_t delay
)
{
    debug_printf("keyboard repeat delay=%d, rate=%d\n", delay, rate);
}

static const struct wl_keyboard_listener keyboard_listener =
{
    .keymap      = keyboard_keymap,
    .enter       = keyboard_enter,
    .leave       = keyboard_leave,
    .key         = keyboard_key,
    .modifiers   = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

#pragma endregion

void keyboard_init(struct keyboard *keyboard, struct seat *seat)
{
    assert(keyboard != NULL && seat != NULL);

    keyboard->seat = seat;

    hashset_init(&keyboard->surface_visited);
}

void keyboard_cleanup(struct keyboard *keyboard)
{
    assert(keyboard != NULL);

    if (keyboard->xkb_state)
    {
        xkb_state_unref(keyboard->xkb_state);
        keyboard->xkb_state = NULL;
    }
    if (keyboard->xkb_keymap)
    {
        xkb_keymap_unref(keyboard->xkb_keymap);
        keyboard->xkb_keymap = NULL;
    }
    keyboard->has_xkb_keymap = false;

    struct hashset_iterator iterator_surfaces = { };
    struct double_buffer_surface *surface = NULL;
    hashset_iterator_init(&iterator_surfaces, &keyboard->surface_visited);
    while ((surface = hashset_iterator_next_deletion(&iterator_surfaces)))
        double_buffer_surface_remove_keyboard_state_by_seat(surface, keyboard->seat);
    hashset_cleanup(&keyboard->surface_visited);
}

void keyboard_assign_wl_keyboard(struct keyboard *keyboard, struct wl_keyboard *wl_keyboard)
{
    assert(keyboard != NULL && wl_keyboard != NULL);

    keyboard_remove_wl_keyboard(keyboard);

    keyboard->wl_keyboard = wl_keyboard;
    wl_keyboard_add_listener(keyboard->wl_keyboard, &keyboard_listener, keyboard);

    keyboard->xkb_keymap = NULL;
    keyboard->xkb_state  = NULL;
    keyboard->has_xkb_keymap = false;

    keyboard->available = true;
}

void keyboard_remove_wl_keyboard(struct keyboard *keyboard)
{
    assert(keyboard != NULL);

    if (!keyboard->wl_keyboard)
        return;

    if (wl_keyboard_get_version(keyboard->wl_keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
        wl_keyboard_release(keyboard->wl_keyboard);
    else
        wl_keyboard_destroy(keyboard->wl_keyboard);

    keyboard->wl_keyboard = NULL;

    // TODO: release all keyboard inputs to surfaces?
    // act as leaving current interacting surface
    if (keyboard->surface_curr)
    {
        struct keyboard_state *keyboard_state = double_buffer_surface_get_keyboard_state(keyboard->surface_curr, keyboard->seat);

        if (keyboard_state)
            keyboard_state_leave(keyboard_state);
    }

    keyboard->removed = true;
}

void keyboard_manage_post_update(struct keyboard *keyboard)
{
    assert(keyboard != NULL);

    struct hashset_iterator iterator_surfaces = { };
    struct double_buffer_surface *surface = NULL;
    hashset_iterator_init(&iterator_surfaces, &keyboard->surface_visited);
    while ((surface = hashset_iterator_next(&iterator_surfaces)))
    {
        struct keyboard_state *keyboard_state = double_buffer_surface_get_keyboard_state(surface, keyboard->seat);
        
        if (keyboard_state)
            keyboard_state_manage_post_update(keyboard_state);
    }

    if (keyboard->removed)
        keyboard->available = false;
    
    keyboard->removed = false;
}
