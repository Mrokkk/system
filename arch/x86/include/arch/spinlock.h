#pragma once

#include <kernel/spinlock.h>

static inline void spinlock_lock(spinlock_t* lock)
{
    register int dummy = SPINLOCK_LOCKED;

    asm volatile(
        "1: "
        "xchgl %0, %1;"
        "test %1, %1;"
        "rep nop;"
        "jnz 1b;"
        : "=m" (lock->lock)
        : "r" (dummy)
        : "memory");
}

static inline void spinlock_unlock(spinlock_t* lock)
{
    register int dummy = SPINLOCK_UNLOCKED;

    asm volatile(
        "xchgl %0, %1;"
        : "=m" (lock->lock)
        : "r" (dummy)
        : "memory");
}
