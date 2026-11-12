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

#include "Utility.h"

#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>

#include <unistd.h>

wl_fixed_t Utility_DistSqBetweenVecWlFixed(wl_fixed_t x1, wl_fixed_t y1, wl_fixed_t x2, wl_fixed_t y2)
{
    int64_t diff_x = x2 - x1;
    int64_t diff_y = y2 - y1;
    return (wl_fixed_t)((((diff_x * diff_x) + (diff_y * diff_y)) + 128) >> 8);
}

int Utility_WlFixedToInt(wl_fixed_t wl_fixed)
{
    return (wl_fixed + 128) >> 8;
}

bool Utility_IsStrStartsWith(const char *str_to_cmp, const char *str)
{
    if (!(str_to_cmp && str))
        return false;

    while (*str)
    {
        // differs
        if (*str_to_cmp != *str)
            return false;

        str_to_cmp++;
        str++;
    }

    return true;
}

bool Utility_IsStrEndsWith(const char *str_to_cmp, const char *str)
{
    if (!(str_to_cmp && str))
        return false;

    size_t len_str_to_cmp = strlen(str_to_cmp);
    size_t len_str = strlen(str);

    if (len_str > len_str_to_cmp)
        return false;

    size_t offset = len_str_to_cmp - len_str;

    return !memcmp(str_to_cmp + offset, str, len_str);
}

char *Utility_DupSubString(const char *str, size_t start_index, size_t length)
{
    if (!str)
        return NULL;

    if (length == 0)
        return NULL;

    size_t len_str = strlen(str);

    if (start_index >= len_str)
        return NULL;

    size_t len_remain = len_str - start_index;
    size_t len_substr = length;
    if (len_remain < len_substr)
        len_substr = len_remain;

    char *substr = malloc(len_substr + 1);
    if (!substr)
        return NULL;

    memcpy(substr, str + start_index, len_substr);
    substr[len_substr] = '\0';

    return substr;
}
