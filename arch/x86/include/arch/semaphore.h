#pragma once

#include <arch/system.h>

static inline void semaphore_up(semaphore_t* sem)
{
    asm volatile(
        "incl 0(%0);"
        "call __semaphore_wake;"
        :: "c" (sem)
        : "memory");
}

static inline void semaphore_down(semaphore_t* sem)
{
    asm volatile(
        "mov $2f, %%eax;"
        "decl 0(%0);"
        "js __semaphore_sleep;"
        "2:"
        :: "c" (sem)
        : "memory", "ax");
}
