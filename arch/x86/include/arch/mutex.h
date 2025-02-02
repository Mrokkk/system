#pragma once

#include <arch/system.h>

static inline int mutex_try_lock(mutex_t* mutex)
{
#if CONFIG_X86 > 3
    int ret = 1;

    asm volatile(
        // First operand is src, second dest. cmpxchg will put src into
        // dest if dest == EAX
        "cmpxchgl %2, %1;"
        : "=a" (ret), "+m" (mutex->count)
        : "q" (0), "a" (1)
        : "memory");

    return !ret;
#else
    scoped_irq_lock();

    if (unlikely(!mutex->count))
    {
        return 1;
    }

    mutex->count = 0;
    return 0;
#endif
}
