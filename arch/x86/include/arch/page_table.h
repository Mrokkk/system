#pragma once

#ifdef __x86_64__
#include "page_table_64.h"
#else
#include "page_table_32.h"
#endif

#define PAGE_PRESENT        (1 << 0)
#define PAGE_RW             (1 << 1)
#define PAGE_USER           (1 << 2)
#define PAGE_PWT            (1 << 3)
#define PAGE_PCD            (1 << 4)
#define PAGE_ACCESSED       (1 << 5)
#define PAGE_DIRTY          (1 << 6)
#define PAGE_GLOBAL         (1 << 7)

#ifdef __x86_64__
#define PAGE_NX             (1 << 63)
#else
#define PAGE_NX             0
#endif

#define PAGE_MMIO   (PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL | PAGE_PCD)
#define PAGE_DIR_KERNEL (PAGE_PRESENT | PAGE_RW | PAGE_USER)

#define kernel_identity_pgprot(flag) \
    ({ \
        PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL | \
            (flag & PAGE_ALLOC_UNCACHED ? PAGE_PCD : 0); \
    })

#ifndef __ASSEMBLER__

static inline void pgd_load(pgd_t* pgd)
{
    asm volatile(
        "mov %0, %%cr3;"
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

#define invlpg(address) \
    asm volatile("invlpg (%0);" :: "r" (address) : "memory")

#define tlb_flush()            pgd_reload()
#define tlb_flush_single(addr) invlpg(addr)

#endif