#pragma once

#include <arch/system.h>
#include <kernel/debug.h>
#include <kernel/compiler.h>

static inline void NORETURN(idle(void))
{
#if PARANOIA_IDLE
    // Verify that context is restored properly after each
    // interruption
    asm volatile(
        "mov $1, %%eax;"
        "mov $2, %%ebx;"
        "mov $3, %%ecx;"
        "mov $4, %%edx;"
        "mov $5, %%esi;"
        "mov $6, %%edi;"
        "1:"
        "hlt;"
        "cmpl $1, %%eax;"
        "jne 2f;"
        "cmp $2, %%ebx;"
        "jne 2f;"
        "cmp $3, %%ecx;"
        "jne 2f;"
        "cmp $4, %%edx;"
        "jne 2f;"
        "cmp $5, %%esi;"
        "jne 2f;"
        "cmp $6, %%edi;"
        "jne 2f;"
        "jmp 1b;"
        "2:"
        "movl $0, 0;" // Just crash to show registers
        ::: "memory");
#else
    for (;; halt());
#endif
    ASSERT_NOT_REACHED();
}
