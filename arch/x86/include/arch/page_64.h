#pragma once

#define PAGES_IN_PTE        512
#define PTE_IN_PDE          1024
#define KERNEL_PAGE_OFFSET  0xffff800000000000
#define INIT_PGT_SIZE       4

#define PDE_PRESENT     (1 << 0)
#define PDE_WRITEABLE   (1 << 1)
#define PDE_USER        (1 << 2)
#define PDE_WRITETHR    (1 << 3)
#define PDE_CACHEDIS    (1 << 4)
#define PDE_ACCESSED    (1 << 5)
#define PDE_RESERVED    (1 << 6)
#define PDE_SIZE        (1 << 7)

#define PTE1_PRESENT        (1 << 0)
#define PTE1_WRITEABLE      (1 << 1)
#define PTE1_USER           (1 << 2)
#define PTE1_WRITETHR       (1 << 3)
#define PTE1_CACHEDIS       (1 << 4)
#define PTE1_ACCESSED       (1 << 5)
#define PTE1_DIRTY          (1 << 6)
#define PTE1_RESERVED       (1 << 7)
#define PTE1_GLOBAL         (1 << 8)

#ifndef __ASSEMBLER__

#ifndef __KERNEL_PAGE_INCLUDED
#error "Include <kernel/page.h>"
#endif

#include <stddef.h>
#include <stdint.h>
#include <kernel/compiler.h>

typedef uintptr_t pgd_t;
typedef uintptr_t pgt_t;

#define pte1_index(vaddr)       ((addr(vaddr) >> (12 + 0 * 9)) & (PAGES_IN_PTE - 1))
#define pte2_index(vaddr)       ((addr(vaddr) >> (12 + 1 * 9)) & (PAGES_IN_PTE - 1))
#define pte3_index(vaddr)       ((addr(vaddr) >> (12 + 2 * 9)) & (PAGES_IN_PTE - 1))
#define pte4_index(vaddr)       ((addr(vaddr) >> (12 + 3 * 9)) & (PAGES_IN_PTE - 1))
#define KERNEL_PTE4_OFFSET      (pte4_index(KERNEL_PAGE_OFFSET))

void clear_first_pde(void);
void page_map_panic(uintptr_t start, uintptr_t end);

#endif // __ASSEMBLER__
