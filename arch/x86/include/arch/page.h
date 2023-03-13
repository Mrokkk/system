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
#define KERNEL_PDE_OFFSET   (KERNEL_PAGE_OFFSET / (PAGE_SIZE * PTE_IN_PDE))

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <kernel/list.h>
#include <kernel/trace.h>
#include <kernel/compiler.h>

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

typedef uint32_t pgd_t;
typedef uint32_t pgt_t;

typedef struct page
{
    uint32_t count;
    void* virtual;
#if TRACE_PAGE_DETAILED
    void* caller;
#endif
    struct list_head list_entry;
} page_t;

extern page_t* page_map;

#define PAGE_ALLOC_DISCONT  0x0
#define PAGE_ALLOC_CONT     0x1

#define page(phys)              (page_map + ((phys) / PAGE_SIZE))
#define pfn(p)                  ((p) - page_map)
#define page_phys(p)            (pfn(p) * PAGE_SIZE)
#define page_phys_ptr(p)        (ptr(pfn(p) * PAGE_SIZE))
#define page_virt(p)            (addr((p)->virtual))
#define page_virt_ptr(p)        ((p)->virtual)
#define phys(virt)              ((typeof(virt))((uint32_t)(virt) - KERNEL_PAGE_OFFSET))
#define virt(phys)              ((typeof(phys))((uint32_t)(phys) + KERNEL_PAGE_OFFSET))
#define virt_ptr(paddr)         (virt(ptr(paddr)))
#define virt_cptr(paddr)        (virt(cptr(paddr)))
#define virt_addr(paddr)        (virt(addr(paddr)))
#define phys_ptr(vaddr)         (phys(ptr(vaddr)))
#define phys_cptr(vaddr)        (phys(cptr(vaddr)))
#define phys_addr(vaddr)        (phys(addr(vaddr)))
#define pde_index(vaddr)        ((uint32_t)(vaddr) >> 22)
#define pte_index(vaddr)        (((uint32_t)(vaddr) >> 12) & 0x3ff)
#define page_align(address)     (((address) + PAGE_MASK) & ~PAGE_MASK)
#define page_beginning(address) ((address) & ~PAGE_MASK)
#define kernel_address(address) ((address) >= KERNEL_PAGE_OFFSET)

#define __phys
#define __kernel_virt
#define __user_virt

// Allocate a page(s) and map it/them in kernel; allocation of
// multiple pages is done according to PAGE_ALLOC_* flags.
// Returned pages are linked together through list_entry (first
// page is list's head)
page_t* __page_alloc(int count, int flags);
int __page_free(void* address);

page_t* __page_range_get(uint32_t paddr, int count);
int __page_range_free(struct list_head* head);

pgd_t* pgd_alloc(pgd_t* old_pgd);
pgt_t* pgt_alloc(void);
pgd_t* init_pgd_get(void);

void page_kernel_identity_map(uint32_t paddr, uint32_t size);

void page_stats_print(void);
void pde_print(const uint32_t entry, char* output);
void pte_print(const uint32_t entry, char* output);

static inline void pgd_load(pgd_t* pgd)
{
    asm volatile(
        "mov %0, %%cr3;"
        "mov $1f, %0;"
        "jmp *%0;"
        "1:"
        :: "r" (pgd) : "memory");
}

static inline void pgd_reload(void)
{
    register int dummy = 0;
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

#define page_range_last(p) \
    list_entry(p->list_entry.prev, page_t, list_entry)

#define single_page() \
    ({ page_t* res = page_alloc(1, PAGE_ALLOC_CONT); res ? res->virtual : NULL; })

#define page_range_for_each(p)

#if TRACE_PAGE

#define page_free(ptr) \
    ({ typecheck_ptr(ptr); \
       int errno = __page_free(ptr); if (likely(!errno)) { log_debug("[FREE] %x", ptr); } errno; })

#define frame_free(addr) \
    ({ int count = __frame_free(addr); log_debug("[FRAME PUT] %x; count=%u", addr, count);  count; })

#define page_alloc(c, f) \
    ({ page_t* res = __page_alloc(c, f); \
       if (likely(res)) { log_debug("[PALLOC] paddr=%x vaddr=%x count=%u", page_phys(res), res->virtual, c); } \
       res; \
    })

#define page_range_get(p, c) \
    ({ page_t* res = __page_range_get(p, c); \
       if (likely(res)) { log_debug("[PGET] paddr=%x vaddr=%x count=%u", page_phys(res), res->virtual, c); } \
       res; \
    })

#define page_range_free(h) \
    ({ \
       typecheck(struct list_head*, h); \
       int c = __page_range_free(h); \
       if (likely(c)) { log_debug("[PFREE] count=%u", c); } \
       c; \
    })

#define page_alloc1() \
    ({ __page_alloc(1, PAGE_ALLOC_DISCONT); })

#else

#define page_free(ptr) __page_free(ptr)
#define frame_get(addr) __frame_get(addr)
#define frame_free(addr) __frame_free(addr)
#define page_alloc(c, f) __page_alloc(c, f)
#define page_range_free(h) __page_range_free(h)
#define page_range_get(p, c) __page_range_get(p, c)
#define page_alloc1() __page_alloc(1, PAGE_ALLOC_DISCONT)

#endif // TRACE_PAGE

#endif // __ASSEMBLER__
