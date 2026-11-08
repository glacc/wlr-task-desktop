#include "Utility.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include <unistd.h>

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
