#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kernel/time.h>
#include <kernel/mutex.h>
#include <kernel/minmax.h>
#include <kernel/api/ioctl.h>
#include <kernel/api/types.h>

#define FB_FLAGS_VIRTFB 1

struct fb_rect
{
    uint16_t x, y, w, h;
};

typedef struct fb_rect fb_rect_t;

struct fb_ops
{
    void (*dirty_set)(const fb_rect_t* rect);
    int (*mode_set)(int resx, int resy, int bpp);
    void (*refresh)(void);
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
    int         flags;
    timeval_t   delay;
    timer_t     timer;
    mutex_t     lock;
    fb_rect_t   dirty;

    fb_ops_t*   ops;
};

typedef struct framebuffer framebuffer_t;

#define fb_writeb(val, addr) (*(volatile uint8_t*)(addr) = (val))
#define fb_writew(val, addr) (*(volatile uint16_t*)(addr) = (val))
#define fb_writel(val, addr) (*(volatile uint32_t*)(addr) = (val))

extern framebuffer_t framebuffer;

int framebuffer_client_register(void (*callback)(void* data), void* data);
void framebuffer_refresh(void);

static inline void fb_rect_enlarge(fb_rect_t* rect, const fb_rect_t* new_rect)
{
    if (rect->w == 0 && rect->h == 0)
    {
        *rect = *new_rect;
        return;
    }

    size_t x0 = rect->x;
    size_t x1 = rect->x + rect->w;

    size_t y0 = rect->y;
    size_t y1 = rect->y + rect->h;

    x0 = min(x0, new_rect->x);
    x1 = max(x1, new_rect->x + new_rect->w);

    y0 = min(y0, new_rect->y);
    y1 = max(y1, new_rect->y + new_rect->h);

    rect->x = x0;
    rect->y = y0;
    rect->w = x1 - x0;
    rect->h = y1 - y0;
}

static inline void fb_rect_clear(fb_rect_t* rect)
{
    memset(rect, 0, sizeof(*rect));
}
