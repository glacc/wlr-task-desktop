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

#include "debug.h"

#include <stdio.h>
#include <stdarg.h>

static const char debug_null_str[] = "%%NULL%%";

const char *debug_str_nullable(const char *str)
{
    if (!str)
        return debug_null_str;
    
    return str;
}

void debug_printf(const char *format, ...)
{
#ifndef NDEBUG
    va_list args;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
#endif
}
