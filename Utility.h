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

#pragma once

#include <stddef.h>

#include <wayland-util.h>

#include <SWRender/SWRender.h>

extern wl_fixed_t Utility_DistSqBetweenVecWlFixed(wl_fixed_t x1, wl_fixed_t y1, wl_fixed_t x2, wl_fixed_t y2);
extern int Utility_WlFixedToInt(wl_fixed_t wl_fixed);

extern bool Utility_IsStrStartsWith(const char *str_to_cmp, const char *str);
extern bool Utility_IsStrEndsWith(const char *str_to_cmp, const char *str);
extern char *Utility_DupSubString(const char *str, size_t start_index, size_t length);
