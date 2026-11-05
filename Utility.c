#include "Utility.h"

#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>

#include "SWRender/SWRender.h"

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

void Utility_AlphaPremultiplySWRenderBuffer(SWRenderBuffer *buffer)
{
    
}
