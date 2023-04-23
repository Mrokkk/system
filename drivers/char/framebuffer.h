#pragma once

#include <stdint.h>
#include <stddef.h>

#define FB_TYPE_RGB 1
#define FB_TYPE_TEXT 2

typedef struct
{
    uint8_t* fb;
    size_t size;
    size_t pitch;
    size_t width;
    size_t height;
    uint8_t bpp;
    uint8_t type;
} framebuffer_t;

extern framebuffer_t framebuffer;
