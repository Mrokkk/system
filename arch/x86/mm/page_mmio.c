#define log_fmt(fmt) "page: " fmt
#include <arch/msr.h>
#include <arch/pat.h>
#include <arch/mtrr.h>
#include <arch/cache.h>
#include <arch/cpuid.h>
#include <kernel/list.h>
#include <kernel/ksyms.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>
#include <kernel/page_debug.h>
#include <kernel/page_table.h>

#define DEBUG_PAGE_MMIO 0

struct mmio_region
{
    const char* name;
    uintptr_t   start;
    uintptr_t   end;
    uintptr_t   paddr;
};

typedef struct mmio_region mmio_region_t;

static mmio_region_t* regions;
static mmio_region_t* last_region;

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

UNMAP_AFTER_INIT void page_mmio_init(void)
{
    uint64_t vaddr;

    for (vaddr = KERNEL_MMIO_START;
        vaddr < KERNEL_MMIO_END;
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
    }

    return;

error:
    panic("page_mmio_init: cannot allocate page for tables for vaddr %p", (uintptr_t)vaddr);
}

void* mmio_map(uintptr_t paddr, uintptr_t size, int flags, const char* name)
{
    uintptr_t vaddr;
    mmio_region_t* region;
    pgprot_t pgprot = PAGE_GLOBAL | PAGE_RW | PAGE_PRESENT;
    pgprot_t temp;

    size = page_align(size);

    if (!regions)
    {
        region = regions = single_page();
        if (!regions)
        {
            return NULL;
        }
        memset(region, 0, PAGE_SIZE);
        vaddr = addr(KERNEL_MMIO_END - size);
    }
    else
    {
        vaddr = last_region->start - size;
        region = last_region + 1;
    }

    if (unlikely(vaddr < KERNEL_MMIO_START))
    {
        log_error("mmio_map: cannot map region with size %u; no more space", size);
        return NULL;
    }

    region->start = vaddr;
    region->end = vaddr + size;
    region->name = name;
    region->paddr = paddr;

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

    log_debug(DEBUG_PAGE_MMIO, "mapping %p => %p; size = %lx; prot = %x", paddr, vaddr, size, pgprot);

    if (unlikely(page_mmio_map(paddr, vaddr, size, pgprot)))
    {
        log_warning("mapping failed");
        return NULL;
    }

    last_region = region;

    return ptr(vaddr);
}

void mmio_print(void)
{
    if (regions)
    {
        for (int i = 0;; ++i)
        {
            if (!regions[i].start)
            {
                break;
            }
            log_info("region: [%p - %p] paddr: %p size: %p name: %s",
                regions[i].start,
                regions[i].end - 1,
                regions[i].paddr,
                regions[i].end - regions[i].start,
                regions[i].name);
        }
    }
}
