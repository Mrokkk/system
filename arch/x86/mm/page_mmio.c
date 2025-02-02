#define log_fmt(fmt) "page_mmio: " fmt
#include <arch/msr.h>
#include <arch/pat.h>
#include <arch/mtrr.h>
#include <arch/cache.h>
#include <arch/cpuid.h>

#include <kernel/vm.h>
#include <kernel/list.h>
#include <kernel/ksyms.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>
#include <kernel/page_debug.h>
#include <kernel/page_table.h>

#define DEBUG_PAGE_MMIO 0

typedef struct mmio_region mmio_region_t;

struct mmio_region
{
    uint64_t       start, end;
    const char*    name;
    uintptr_t      paddr;
    mmio_region_t* next;
    mmio_region_t* prev;
};

static mmio_region_t* regions;

static int page_mmio_map(uintptr_t paddr_start, uintptr_t vaddr_start, size_t size, pgprot_t pgprot)
{
    uintptr_t vaddr, paddr;

    for (paddr = paddr_start, vaddr = vaddr_start;
        paddr < paddr_start + size;
        paddr += PAGE_SIZE, vaddr += PAGE_SIZE)
    {
        pgd_t* pgde = pgd_offset(kernel_page_dir, vaddr);
        pud_t* pude = pud_alloc(pgde, vaddr);

        if (unlikely(!pude))
        {
            return -ENOMEM;
        }

        pmd_t* pmde = pmd_alloc(pude, vaddr);

        if (unlikely(!pmde))
        {
            return -ENOMEM;
        }

        pte_t* pte = pte_alloc(pmde, vaddr);

        if (unlikely(!pte))
        {
            return -ENOMEM;
        }

        pte_entry_set(pte, paddr, pgprot);
    }

    return 0;
}

static void* mmio_free_space_find(size_t size)
{
    mmio_region_t* region;
    uintptr_t vaddr, next_start = 0;

    if (!regions)
    {
        return ptr(KERNEL_MMIO_END - size);
    }

    for (region = regions; region->next; region = region->next);

    for (; region; next_start = region->start, region = region->prev)
    {
        if (next_start && next_start - region->end >= size)
        {
            return ptr(region->end);
        }
    }

    if ((vaddr = regions->start - size) < KERNEL_MMIO_START)
    {
        log_error("mmio_map: cannot map region with size %u; no more space", size);
        return NULL;
    }

    return ptr(vaddr);
}

static mmio_region_t* mmio_region_create(void* vaddr, uintptr_t paddr, size_t size, const char* name)
{
    mmio_region_t* region = zalloc(mmio_region_t);

    if (unlikely(!region))
    {
        return NULL;
    }

    region->start = addr(vaddr);
    region->end   = (uint64_t)region->start + size;
    region->paddr = paddr;
    region->name  = name;

    return region;
}

static void mmio_region_add(mmio_region_t* new_region)
{
    uint64_t new_end = new_region->end;
    uint64_t new_start = new_region->start;

    if (!regions)
    {
        regions = new_region;
        new_region->prev = NULL;
        return;
    }

    if (regions->start >= new_end)
    {
        new_region->next = regions;
        new_region->prev = NULL;
        regions->prev = new_region;
        regions = new_region;
        return;
    }

    mmio_region_t* next;
    mmio_region_t* prev = NULL;

    for (mmio_region_t* temp = regions; temp; prev = temp, temp = temp->next)
    {
        if (temp->next && temp->end <= new_start && temp->next->start >= new_end)
        {
            next = temp->next;
            temp->next = new_region;
            new_region->next = next;
            new_region->prev = temp;
            next->prev = new_region;
            return;
        }
    }

    prev->next = new_region;
    new_region->prev = prev;
    new_region->next = NULL;
}

void* mmio_map(uintptr_t paddr, uintptr_t size, int flags, const char* name)
{
    void* ptr;
    mmio_region_t* region;
    pgprot_t temp, pgprot = PAGE_GLOBAL | PAGE_RW | PAGE_PRESENT;

    size = page_align(size);

    if (unlikely(!(ptr = mmio_free_space_find(size))))
    {
        return NULL;
    }

    if (unlikely(!(region = mmio_region_create(ptr, paddr, size, name))))
    {
        return NULL;
    }

    switch (flags)
    {
        case MMIO_UNCACHED:
            pgprot |= pat_enabled()
                ? pat_pgprot_get(CACHE_UC)
                : PAGE_PCD;
            break;

        case MMIO_WRITE_COMBINE:
            temp = pat_enabled()
                ? pat_pgprot_get(CACHE_WC)
                : 0;

            if (!temp && mtrr_enabled())
            {
                if (mtrr_add(paddr, size, CACHE_WC))
                {
                    log_info("cannot apply MTRR for %p", paddr);
                    temp = PAGE_PCD; // Fallback to uncached
                }
            }

            pgprot |= temp;
            break;

        case MMIO_WRITE_BACK:
            pgprot |= pat_enabled()
                ? pat_pgprot_get(CACHE_WB)
                : 0;
            break;
    }

    log_debug(DEBUG_PAGE_MMIO, "mapping %p => %p; size = %#lx; prot = %#x", paddr, ptr, size, pgprot);

    if (unlikely(page_mmio_map(paddr, addr(ptr), size, pgprot)))
    {
        log_warning("mapping failed");
        delete(region);
        return NULL;
    }

    mmio_region_add(region);

    return ptr;
}

static void page_mmio_unmap(uint64_t vaddr_start, uint64_t vaddr_end)
{
    uint64_t vaddr;

    for (vaddr = vaddr_start;
        vaddr < vaddr_end;
        vaddr += PAGE_SIZE)
    {
        pgd_t* pgde = pgd_offset(kernel_page_dir, (uintptr_t)vaddr);
        pud_t* pude = pud_alloc(pgde, (uintptr_t)vaddr);

        if (unlikely(!pude))
        {
            goto error;
        }

        pmd_t* pmde = pmd_alloc(pude, (uintptr_t)vaddr);

        if (unlikely(!pmde))
        {
            goto error;
        }

        pte_t* pte = pte_alloc(pmde, (uintptr_t)vaddr);

        if (unlikely(!pte))
        {
            goto error;
        }

        pte_entry_set(pte, 0, 0);
        tlb_flush_single((uintptr_t)vaddr);
    }

    return;

error:
    panic("page_mmio_unmap: cannot allocate page for tables for vaddr %p", (uintptr_t)vaddr);
}

void mmio_unmap(void* vaddr)
{
    mmio_region_t* region;

    if (unlikely(!regions))
    {
        log_warning("mmio_unmap: cannot unmap %p", vaddr);
        return;
    }

    for (region = regions; region; region = region->next)
    {
        if (region->start == (uintptr_t)vaddr)
        {
            if (region->next) region->next->prev = region->prev;
            if (region->prev) region->prev->next = region->next;

            log_debug(DEBUG_PAGE_MMIO, "unmapping %p => %p; size = %#zx, name = %s",
                region->paddr,
                (uintptr_t)region->start,
                (size_t)(region->end - region->start),
                region->name);

            page_mmio_unmap(region->start, region->end);
            delete(region);
            return;
        }
    }

    log_warning("mmio_unmap: %p is not mapped", vaddr);
}

void mmio_print(void)
{
    if (!regions)
    {
        return;
    }

    for (mmio_region_t* region = regions; region; region = region->next)
    {
        log_info("[%p - %p] paddr: %p size: %#zx name: %s",
            ptr(region->start),
            ptr(region->end - 1),
            ptr(region->paddr),
            (size_t)(region->end - region->start),
            region->name);
    }
}

UNMAP_AFTER_INIT void page_mmio_init(void)
{
    page_mmio_unmap(KERNEL_MMIO_START, KERNEL_MMIO_END);
}
