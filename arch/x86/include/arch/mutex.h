#pragma once

#include <arch/system.h>

static inline int mutex_try_lock(mutex_t* mutex)
{
    int ret = 1;

    asm volatile(
        // First operand is src, second dest. cmpxchg will put src into
        // dest if dest == EAX
        "cmpxchgl %2, %1;"
        : "=a" (ret), "+m" (mutex->count)
        : "q" (0), "a" (1)
        : "memory");

    return !ret;
}
