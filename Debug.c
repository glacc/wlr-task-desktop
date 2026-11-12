#include "Debug.h"

#include <stdio.h>
#include <stdarg.h>

void Debug_Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
