#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
}
__attribute__((packed)) SWRenderColor;

#define SWRENDER_EXTIMAGE_AVPIXFMT AV_PIX_FMT_BGRA
