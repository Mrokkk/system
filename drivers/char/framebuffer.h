#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint8_t* fb;
    size_t size;
    size_t pitch;
    size_t width;
    size_t height;
    uint8_t bpp;
} framebuffer_t;

extern framebuffer_t framebuffer;
