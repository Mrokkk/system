#pragma once

#include <stdint.h>
#include <kernel/list.h>
#include <kernel/page_debug.h>

#include <arch/page_types.h>

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

typedef struct page page_t;
