#pragma once

#include <stdint.h>

struct fb_var_screeninfo;

struct tga_header
{
    uint8_t magic1;
    uint8_t colormap;
    uint8_t encoding;
    uint16_t cmaporig, cmaplen;
    uint8_t cmapent;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t bpp;
    uint8_t alpha_bits:4;
    uint8_t pixel_ordering:2;
    uint8_t reserved:2;
} __attribute__((packed));

int tga_to_framebuffer(
    struct tga_header* base,
    int x,
    int y,
    struct fb_var_screeninfo* vinfo,
    uint8_t* framebuffer);
