#pragma once

// Some note regarding naming convention:
// PGD (PaGe Directory) has 1024 PDEs (Page Directory Entry). Each PDE points to
// a PGT (PaGe Table) which has 1024 PTEs (Page Table Entry). Each PTE points to
// a physical page (frame)

#define PAGE_SIZE           0x1000
#define PAGE_MASK           0xfff
#define PAGE_ADDRESS        (~PAGE_MASK)
#define PAGE_NUMBER         0x100000
#define PAGES_IN_PTE        1024
#define PTE_IN_PDE          1024
#define PAGE_KERNEL_FLAGS   0x3
#define KERNEL_PAGE_OFFSET  0xc0000000
#define ADDRESS_SPACE       0xffffffff
#define INIT_PGT_SIZE       4
#define KERNEL_PDE_OFFSET   (KERNEL_PAGE_OFFSET / (PAGE_SIZE * PTE_IN_PDE))

#define PDE_PRESENT     (1 << 0)
#define PDE_WRITEABLE   (1 << 1)
#define PDE_USER        (1 << 2)
#define PDE_WRITETHR    (1 << 3)
#define PDE_CACHEDIS    (1 << 4)
#define PDE_ACCESSED    (1 << 5)
#define PDE_RESERVED    (1 << 6)
#define PDE_SIZE        (1 << 7)

#define PTE_PRESENT     (1 << 0)
#define PTE_WRITEABLE   (1 << 1)
#define PTE_USER        (1 << 2)
#define PTE_WRITETHR    (1 << 3)
#define PTE_CACHEDIS    (1 << 4)
#define PTE_ACCESSED    (1 << 5)
#define PTE_DIRTY       (1 << 6)
#define PTE_RESERVED    (1 << 7)
#define PTE_GLOBAL      (1 << 8)

#ifndef __ASSEMBLER__

#ifndef __KERNEL_PAGE_INCLUDED
#error "Include <kernel/page.h>"
#endif

#include <stddef.h>
#include <stdint.h>
#include <kernel/compiler.h>

typedef uintptr_t pgd_t;
typedef uintptr_t pgt_t;

#define pde_index(vaddr)        (addr(vaddr) >> 22)
#define pte_index(vaddr)        ((addr(vaddr) >> 12) & 0x3ff)

void pde_print(const uint32_t entry, char* output, size_t size);
void pte_print(const uint32_t entry, char* output, size_t size);

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

#define pte_flags(vaddr, pgd) \
    ({ \
        uintptr_t flags = 0; \
        uintptr_t pde_index = pde_index(addr(vaddr)); \
        uintptr_t pte_index = pte_index(addr(vaddr)); \
        if (pgd[pde_index]) \
        { \
            pgt_t* pgt = virt_ptr(pgd[pde_index] & ~PAGE_MASK); \
            flags = pgt[pte_index] & PAGE_MASK; \
        } \
        flags; \
    })

void clear_first_pde(void);
void page_map_panic(uintptr_t start, uintptr_t end);

#endif // __ASSEMBLER__
