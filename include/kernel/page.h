#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kernel/list.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/compiler.h>

#define __KERNEL_PAGE_INCLUDED
#include <arch/page.h>

#define DEBUG_PAGE          0
#define DEBUG_PAGE_DETAILED 0 // Collects page_alloc caller address

typedef struct page page_t;
typedef struct vm_region vm_region_t;

struct page
{
    uint16_t    refcount;
    size_t      pages_count;
    void*       virtual;
#if DEBUG_PAGE_DETAILED
    void*       caller;
#endif
    list_head_t list_entry;
};

struct vm_region
{
    const char*    name;
    memory_area_t* area;
    uintptr_t      start;
    uintptr_t      end;
    uintptr_t      paddr;
};

#define page(phys)              ({ extern page_t* page_map; page_map + ((phys) / PAGE_SIZE); })
#define pfn(p)                  ({ extern page_t* page_map; (p) - page_map; })
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
#define page_align(address)     align(address, PAGE_SIZE)
#define page_beginning(address) ((address) & ~PAGE_MASK)
#define kernel_address(address) ((address) >= KERNEL_PAGE_OFFSET)

typedef enum
{
    PAGE_ALLOC_DISCONT = 0,
    PAGE_ALLOC_CONT = 1,
    PAGE_ALLOC_UNCACHED = 2,
    PAGE_ALLOC_NO_KERNEL = 4,
} alloc_flag_t;

// Allocate a page(s) and map it/them in kernel; allocation of
// multiple pages is done according to PAGE_ALLOC_* flags.
// Returned pages are linked together through list_entry (first
// page is list's head)
MUST_CHECK(page_t*) __page_alloc(int count, alloc_flag_t flag);
int __page_free(void* address); // FIXME: remove this in the future
int __pages_free(page_t* pages);

page_t* pages_split(page_t* pages, size_t pages_to_keep);

MUST_CHECK(pgd_t*) pgd_alloc(void);
MUST_CHECK(pgt_t*) pgt_alloc(void);
MUST_CHECK(pgd_t*) init_pgd_get(void);

void* region_map(uintptr_t paddr, uintptr_t size, const char* name);

void page_stats_print(void);

#define page_range_last(p) \
    list_entry(p->list_entry.prev, page_t, list_entry)

#define single_page() \
    ({ page_t* res = page_alloc(1, PAGE_ALLOC_DISCONT); res ? res->virtual : NULL; })

#if DEBUG_PAGE

#define page_free(ptr) \
    ({ typecheck_ptr(ptr); \
       int errno = __page_free(ptr); if (likely(!errno)) { log_debug(DEBUG_PAGE, "[FREE] %x", ptr); } errno; })

#define pages_free(h) \
    ({ \
       typecheck(page_t*, h); \
       int c = __pages_free(h); \
       if (likely(c)) { log_debug(DEBUG_PAGE, "[PFREE] count=%u", c); } \
       c; \
    })

#define page_alloc(c, f) \
    ({ page_t* res = __page_alloc(c, f); \
       if (likely(res)) { log_debug(DEBUG_PAGE, "[PALLOC] paddr=%x vaddr=%x count=%u", page_phys(res), res->virtual, c); } \
       res; \
    })

#else

#define page_free(ptr)          __page_free(ptr)
#define pages_free(ptr)         __pages_free(ptr)
#define page_alloc(c, f)        __page_alloc(c, f)

#endif // DEBUG_PAGE

#define page_alloc1()           page_alloc(1, PAGE_ALLOC_DISCONT)
