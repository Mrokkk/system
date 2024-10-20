#pragma once

#define PAGE_SIZE           0x1000
#define PAGE_MASK           0xfff
#define PAGE_ADDRESS        (~PAGE_MASK)
#define PAGE_KERNEL_FLAGS   0x3

#ifdef __i386__
#include "page_32.h"
#else
#include "page_64.h"
#endif

#ifndef __ASSEMBLER__

static inline void pgd_load(pgd_t* pgd)
{
    asm volatile(
        "mov %0, %%cr3;"
        "mov $1f, %0;"
        "jmp *%0;"
        "1:"
        :: "r" ((uintptr_t)pgd - KERNEL_PAGE_OFFSET) : "memory");
}

static inline void pgd_reload(void)
{
    register long dummy = 0;
    asm volatile(
        "mov %%cr3, %0;"
        "mov %0, %%cr3;"
        : "=r" (dummy)
        : "r" (dummy)
        : "memory"
    );
}

static inline void invlpg(void* address)
{
    asm volatile(
        "invlpg (%0);"
        :: "r" (address)
        : "memory"
    );
}

#endif // !__ASSEMBLER__
