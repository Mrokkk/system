#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kernel/list.h>
#include <kernel/trace.h>
#include <kernel/compiler.h>

#define __KERNEL_PAGE_INCLUDED
#include <arch/page.h>

typedef struct page
{
    size_t count;
    void* virtual;
#if DEBUG_PAGE_DETAILED
    void* caller;
#endif
    struct list_head list_entry;
} page_t;

extern page_t* page_map;

#define page(phys)              (page_map + ((phys) / PAGE_SIZE))
#define pfn(p)                  ((p) - page_map)
#define page_phys(p)            (pfn(p) * PAGE_SIZE)
#define page_phys_ptr(p)        (ptr(pfn(p) * PAGE_SIZE))
#define page_virt(p)            (addr((p)->virtual))
#define page_virt_ptr(p)        ((p)->virtual)
#define phys(virt)              ((typeof(virt))(addr(virt) - KERNEL_PAGE_OFFSET))
#define virt(phys)              ((typeof(phys))(addr(phys) + KERNEL_PAGE_OFFSET))
#define virt_ptr(paddr)         (virt(ptr(paddr)))
#define virt_cptr(paddr)        (virt(cptr(paddr)))
#define virt_addr(paddr)        (virt(addr(paddr)))
#define phys_ptr(vaddr)         (phys(ptr(vaddr)))
#define phys_cptr(vaddr)        (phys(cptr(vaddr)))
#define phys_addr(vaddr)        (phys(addr(vaddr)))
#define page_align(address)     (((address) + PAGE_MASK) & ~PAGE_MASK)
#define page_beginning(address) ((address) & ~PAGE_MASK)
#define kernel_address(address) ((address) >= KERNEL_PAGE_OFFSET)

#define PAGE_ALLOC_DISCONT  0x0
#define PAGE_ALLOC_CONT     0x1

// Allocate a page(s) and map it/them in kernel; allocation of
// multiple pages is done according to PAGE_ALLOC_* flags.
// Returned pages are linked together through list_entry (first
// page is list's head)
page_t* __page_alloc(int count, int flags);
int __page_free(void* address);

page_t* __page_range_get(uint32_t paddr, int count);
int __page_range_free(struct list_head* head);

pgd_t* pgd_alloc(void);
pgt_t* pgt_alloc(void);
pgd_t* init_pgd_get(void);

void page_kernel_identity_map(uint32_t paddr, uint32_t size);

void page_stats_print(void);

#define page_range_last(p) \
    list_entry(p->list_entry.prev, page_t, list_entry)

#define single_page() \
    ({ page_t* res = page_alloc(1, PAGE_ALLOC_CONT); res ? res->virtual : NULL; })

#define page_range_for_each(p)

#if DEBUG_PAGE

#define page_free(ptr) \
    ({ typecheck_ptr(ptr); \
       int errno = __page_free(ptr); if (likely(!errno)) { log_debug(DEBUG_PAGE, "[FREE] %x", ptr); } errno; })

#define page_alloc(c, f) \
    ({ page_t* res = __page_alloc(c, f); \
       if (likely(res)) { log_debug(DEBUG_PAGE, "[PALLOC] paddr=%x vaddr=%x count=%u", page_phys(res), res->virtual, c); } \
       res; \
    })

#define page_range_get(p, c) \
    ({ page_t* res = __page_range_get(p, c); \
       if (likely(res)) { log_debug(DEBUG_PAGE, "[PGET] paddr=%x vaddr=%x count=%u", page_phys(res), res->virtual, c); } \
       res; \
    })

#define page_range_free(h) \
    ({ \
       typecheck(struct list_head*, h); \
       int c = __page_range_free(h); \
       if (likely(c)) { log_debug(DEBUG_PAGE, "[PFREE] count=%u", c); } \
       c; \
    })

#define page_alloc1() \
    ({ __page_alloc(1, PAGE_ALLOC_DISCONT); })

#else

#define page_free(ptr) __page_free(ptr)
#define page_alloc(c, f) __page_alloc(c, f)
#define page_range_free(h) __page_range_free(h)
#define page_range_get(p, c) __page_range_get(p, c)
#define page_alloc1() __page_alloc(1, PAGE_ALLOC_DISCONT)

#endif // DEBUG_PAGE
