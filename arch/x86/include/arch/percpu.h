#pragma once

#include <kernel/compiler.h>

#define PER_CPU_DECLARE(...) \
    SECTION(.data.per_cpu) __VA_ARGS__

#define THIS_CPU_GET(data) \
    ({ \
        typeof(&data) ret; \
        asm volatile( \
            "mov %%fs:0x0, %0" \
            : "=r" (ret) \
            :: "memory"); \
        shift(ret, ptr(&data) - ptr(_data_per_cpu_start)); \
    })

#define CPU_GET(id, var) \
    ({ \
        shift_as(typeof(var), per_cpu_data[id], addr(var) - addr(_data_per_cpu_start)); \
    })

void bsp_per_cpu_setup(void);

extern void* per_cpu_data[];
