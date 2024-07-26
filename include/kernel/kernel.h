#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/system.h>
#include <arch/processor.h>

#include <kernel/div.h>
#include <kernel/list.h>
#include <kernel/debug.h>
#include <kernel/magic.h>
#include <kernel/debug.h>
#include <kernel/limits.h>
#include <kernel/malloc.h>
#include <kernel/printk.h>
#include <kernel/string.h>
#include <kernel/compiler.h>
#include <kernel/sections.h>
#include <kernel/api/types.h>
#include <kernel/api/errno.h>

int strtoi(const char* str);
int sprintf(char*, const char*, ...);
int snprintf(char* buf, size_t size, const char* fmt, ...);

int vsprintf(char*, const char*, va_list);
int vsnprintf(char*, size_t, const char*, va_list);

#define fmtstr(buf, ...) \
    ({ sprintf(buf, __VA_ARGS__); buf; })

#define KiB (1024UL)
#define MiB (1024UL * KiB)
#define GiB (1024UL * MiB)

#define human_size(size, unit) \
    ({ \
        unit = "B"; \
        if (size >= GiB) \
        { \
            do_div(size, GiB); \
            unit = "GiB"; \
        } \
        else if (size >= MiB) \
        { \
            do_div(size, MiB); \
            unit = "MiB"; \
        } \
        else if (size >= 2 * KiB) \
        { \
            do_div(size, KiB); \
            unit = "KiB"; \
        } \
    })

#define UNMAP_AFTER_INIT    SECTION(.text.init) NOINLINE
#define __user

#define offset_get(a, b) \
    ({ addr(a) - addr(b); })

#define errno_get(data) \
    ({ \
        int ret = (int)(data); \
        int errno = 0; \
        if (unlikely(ret < 0 && ret >= -ERRNO_MAX)) \
        { \
            errno = ret; \
        } \
        errno; \
    })

void breakpoint(void);

extern volatile unsigned int jiffies;
