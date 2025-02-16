#pragma once

#include <kernel/spinlock.h>

static inline void spinlock_lock(spinlock_t* lock)
{
    asm volatile(
        "1:"
        "lock; btsl $0, %0;"
        "jc 2f;"
        ".section .text.lock, \"ax\";"
        "2:"
        "rep; nop;"
        "testb $1, %0;"
        "jne 2b;"
        "jmp 1b;"
        ".previous;"
        : "=m" (lock->lock)
        :: "memory");
}

static inline void spinlock_unlock(spinlock_t* lock)
{
    asm volatile(
        "lock; btrl $0, %0;"
        : "=m" (lock->lock)
        :: "memory");
}
