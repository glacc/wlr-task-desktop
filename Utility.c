#include "Utility.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

void Utility_TryReplaceWithAnotherString(char **dst, const char *src)
{
    if (!dst)
        return;

    if (*dst)
        free(*dst);

    if (!src)
    {
        *dst = NULL;
        return;
    }

    if (!(*dst = strdup(src)))
        fprintf(stderr, "failed to duplicate src string: %s\n", strerror(errno));
}

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
