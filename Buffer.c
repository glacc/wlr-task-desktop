/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#define _GNU_SOURCE

#include "Buffer.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/mman.h>

#include "Globals.h"

static void Buffer_Release(
    void *data,
    struct wl_buffer *wl_buffer
)
{
    struct Buffer *buffer = data;

    buffer->available = true;
}

static const struct wl_buffer_listener Buffer_listener = 
{
    .release = Buffer_Release,
};

bool Buffer_Init(struct Buffer *buffer, int32_t w, int32_t h)
{
    if (!buffer)
        return false;

    if (!wl_shm)
        return false;

    buffer->buffer = NULL;
    buffer->pixels = NULL;

    // create file for shm
    int fd_shm = memfd_create("memf_wl_shm_buffer", MFD_ALLOW_SEALING);
    struct wl_shm_pool *wl_shm_pool = NULL;
    bool success = false;

    if (fd_shm == -1)
    {
        fprintf(stderr, "failed to create memfd for wl_shm_pool: %s\n", strerror(errno));
        return false;
    }

    size_t size = w * h * 4;

    if (ftruncate(fd_shm, size) == -1)
    {
        fprintf(stderr, "failed to ftruncate fd for wl_shm_pool: %s\n", strerror(errno));
        goto End;
    }

    if ((buffer->pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0)) == MAP_FAILED)
    {
        fprintf(stderr, "failed to map memory from fd: %s\n", strerror(errno));
        goto End;
    }

    // wl shm pool
    if (!(wl_shm_pool = wl_shm_create_pool(wl_shm, fd_shm, size)))
    {
        fprintf(stderr, "failed to create shm pool\n");
        goto End;
    }

    if (!(buffer->buffer = wl_shm_pool_create_buffer(wl_shm_pool, 0, w, h, w * 4, WL_SHM_FORMAT_ARGB8888)))
    {
        fprintf(stderr, "failed to create buffer\n");
        goto End;
    }

    buffer->w = w;
    buffer->h = h;
    buffer->available = true;

    wl_buffer_add_listener(buffer->buffer, &Buffer_listener, buffer);

    success = true;

End:
    close(fd_shm);

    if (wl_shm_pool)
        wl_shm_pool_destroy(wl_shm_pool);

    if (!success && buffer->pixels)
        munmap(buffer->pixels, size);

    return success;
}

void Buffer_Cleanup(struct Buffer *buffer)
{
    if (!buffer)
        return;

    if (buffer->buffer)
    {
        wl_buffer_destroy(buffer->buffer);
        buffer->buffer = NULL;
    }

    if (buffer->pixels)
    {
        munmap(buffer->pixels, buffer->w * buffer->h * 4);
        buffer->pixels = NULL;
    }

    buffer->available = false;
}
