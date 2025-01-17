#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kernel/api/ioctl.h>

struct fb_ops
{
    int (*mode_get)(int resx, int resy, int bpp);
    int (*mode_set)(int mode);
};

typedef struct fb_ops fb_ops_t;

struct framebuffer
{
    const char* id;
    uintptr_t   paddr;
    uint8_t*    vaddr;
    size_t      size;
    size_t      pitch;
    size_t      width;
    size_t      height;
    uint8_t     bpp;
    uint8_t     type;
    uint8_t     type_aux;
    uint8_t     visual;
    int         accel;

    fb_ops_t*   ops;
};

typedef struct framebuffer framebuffer_t;

#define fb_writeb(val, addr) (*(volatile uint8_t*)(addr) = (val))
#define fb_writew(val, addr) (*(volatile uint16_t*)(addr) = (val))
#define fb_writel(val, addr) (*(volatile uint32_t*)(addr) = (val))

extern framebuffer_t framebuffer;

int framebuffer_client_register(void (*callback)(void* data), void* data);
