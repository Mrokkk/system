#pragma once

#include <stdint.h>
#include <sys/ioctl.h>

#define addr(x)             ((uint32_t)(x))
#define ptr(x)              ((void*)(x))
#ifndef offsetof
#define offsetof(st, m)     __builtin_offsetof(st, m)
#endif

static inline int clamp(int value, int min, int max)
{
    if (value >= max)
    {
        value = max - 1;
    }
    else if (value < min)
    {
        value = 0;
    }
    return value;
}

void* file_map(const char* path);

extern struct fb_var_screeninfo vinfo;
extern size_t pitch;
extern uint8_t* buffer;
