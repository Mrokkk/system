#define log_fmt(fmt) "page: " fmt
#include <arch/asm.h>
#include <arch/register.h>

#include <kernel/cpu.h>
#include <kernel/list.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>
#include <kernel/minmax.h>
#include <kernel/process.h>
#include <kernel/sections.h>
#include <kernel/page_alloc.h>
#include <kernel/page_debug.h>
#include <kernel/page_table.h>

extern pte_t page0[];
extern page_t* page_map;
extern list_head_t free_pages;
extern uintptr_t last_pfn;

pte_t* kernel_page_tables;
static mutex_t lock = MUTEX_INIT(lock);

static inline void kernel_pte_set(uintptr_t pte_index, uintptr_t val)
{
    kernel_page_tables[pte_index] = val;
}

static inline void kernel_pde_set(uintptr_t pde_index, uintptr_t val)
{
    kernel_page_dir[pde_index] = val;
}

static void section_free(section_t* section)
{
    page_t* start;
    page_t* end;

    if (section->flags & SECTION_UNPAGED)
    {
        start = page(addr(section->start));
        end = page(addr(section->end));
    }
    else
    {
        start = page(phys_addr(section->start));
        end = page(phys_addr(section->end));
    }

    log_notice("freeing [%08x - %08x] %s", section->start, section->end, section->name);

    for (page_t* page = start; page != end; page++)
    {
        pages_free(page);
    }
}

static void sections_unmap(void)
{
    for (section_t* section = sections; section->name; ++section)
    {
        if (!(section->flags & SECTION_UNMAP_AFTER_INIT))
        {
            continue;
        }

        section_free(section);
    }
}

static UNMAP_AFTER_INIT void rodata_after_init_write_protect()
{
    uintptr_t paddr = phys_addr(_rodata_after_init_start);
    uintptr_t end = phys_addr(_rodata_after_init_end);
    uintptr_t pfn;

    for (; paddr < end; paddr += PAGE_SIZE)
    {
        pfn = paddr / PAGE_SIZE;
        pte_entry_set(kernel_page_tables + pfn, paddr, PAGE_PRESENT);
    }
}

void paging_finalize(void)
{
    rodata_after_init_write_protect();
    sections_unmap();

    kernel_page_dir[0] = 0;

    tlb_flush();
}

pgd_t* pgd_alloc(void)
{
    pgd_t* new_pgd = single_page();

    if (unlikely(!new_pgd))
    {
        return NULL;
    }

    scoped_mutex_lock(&lock);

    memcpy(new_pgd, kernel_page_dir, PAGE_SIZE);

    return new_pgd;
}

int pte_alloc_impl(pmd_t* pmd)
{
    page_t* page = page_alloc(1, PAGE_ALLOC_ZEROED);

    if (unlikely(!page))
    {
        return -ENOMEM;
    }

    pmd_entry_set(pmd, page_phys(page), PAGE_DIR_KERNEL);

    return 0;
}

void page_kernel_map(page_t* page, pgprot_t prot)
{
    scoped_mutex_lock(&lock);

    uintptr_t paddr = page_phys(page);
    uintptr_t vaddr = virt(paddr);
    uintptr_t pfn = paddr / PAGE_SIZE;

    pte_entry_set(kernel_page_tables + pfn, paddr, prot);
    page->virtual = ptr(vaddr);
}

void page_kernel_unmap(page_t* page)
{
    scoped_mutex_lock(&lock);

    uintptr_t paddr = page_phys(page);
    void* vaddr = page_virt_ptr(page);
    uintptr_t pfn = paddr / PAGE_SIZE;

    pte_entry_set(kernel_page_tables + pfn, 0, 0);
    page->virtual = 0;

    tlb_flush_single(vaddr);
}

void page_low_mem_map(pgd_t** pgd)
{
    if (pgd)
    {
        *pgd = virt_ptr(cr3_get());
    }

    kernel_page_dir[0] = kernel_page_dir[KERNEL_PGD_OFFSET];

    for (uintptr_t paddr = 0; paddr < 0x100000; paddr += PAGE_SIZE)
    {
        pte_entry_set(kernel_page_tables + paddr / PAGE_SIZE, paddr, PAGE_PRESENT | PAGE_RW);
    }

    pgd_load(kernel_page_dir);
}

void page_low_mem_unmap(pgd_t* pgd)
{
    kernel_page_dir[0] = 0;

    for (uintptr_t paddr = 0; paddr < 0x100000; paddr += PAGE_SIZE)
    {
        pte_entry_set(kernel_page_tables + paddr / PAGE_SIZE, 0, 0);
        tlb_flush_single(paddr);
    }

    if (pgd)
    {
        pgd_load(pgd);
    }
}

static inline pgprot_t section_to_pgprot(const section_t* section)
{
    int flags = section->flags;
    pgprot_t page_flags = PAGE_PRESENT;
    if (flags & SECTION_WRITE) page_flags |= PAGE_RW;
    if (!(flags & SECTION_EXEC)) page_flags |= PAGE_NX;
    return page_flags;
}

UNMAP_AFTER_INIT size_t page_tables_prealloc(uintptr_t virt_end)
{
    pte_t* prev_pte = page0;
    uintptr_t pte_index;
    uintptr_t pfn_end = last_phys_address / PAGE_SIZE;

    // Set up currently used page tables so it will cover required space to setup pages for 4G
    for (pte_index = phys_addr(_end) / PAGE_SIZE; pte_index < min(INIT_PGT_SIZE * PTRS_PER_PTE, pfn_end); ++pte_index)
    {
        prev_pte[pte_index] = pte_index * PAGE_SIZE | PAGE_PRESENT | PAGE_RW;
    }

    kernel_page_tables = ptr(virt_end);

    size_t size = page_align(min(GiB, page_align(last_phys_address)) / PTRS_PER_PTE);

    memset(kernel_page_tables, 0, size);

    return size;
}

UNMAP_AFTER_INIT void page_tables_init(uintptr_t virt_end)
{
    uintptr_t pte_index, pde_index, flags, start, end = 0;
    uintptr_t pfn_end = phys_addr(virt_end) / PAGE_SIZE;
    section_t* section = sections;

    if (cpu_has(X86_FEATURE_PGE))
    {
        register uintptr_t dummy = 0;
        asm volatile(
            "mov %%cr4, %0;"
            "or "ASM_VALUE(CR4_PGE)", %0;"
            "mov %0, %%cr4;"
            : "=r" (dummy)
            : "r" (dummy));
    }

    for (pte_index = 0; section->name; ++section)
    {
        // Set up gap between sections as not present
        start = section_phys_start(section);
        for (uintptr_t addr = end; addr < start; addr += PAGE_SIZE, ++pte_index)
        {
            kernel_pte_set(pte_index, 0);
        }

        end = section_phys_end(section);
        flags = section_to_pgprot(section);

        // Set up section pages as present with proper protection
        for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE, ++pte_index)
        {
            kernel_pte_set(pte_index, pte_index * PAGE_SIZE | PAGE_GLOBAL | flags);
        }
    }

    for (; pte_index < pfn_end; ++pte_index)
    {
        kernel_pte_set(pte_index, pte_index * PAGE_SIZE | PAGE_PRESENT | PAGE_RW | PAGE_GLOBAL);
    }

    // Set up page directory
    for (pde_index = KERNEL_PGD_OFFSET, pte_index = 0;
        pde_index < min(align(last_phys_address / PAGE_SIZE, PTRS_PER_PTE) / PTRS_PER_PTE + KERNEL_PGD_OFFSET, PTRS_PER_PMD);
        pde_index++, pte_index += PTRS_PER_PMD)
    {
        kernel_pde_set(pde_index, phys_addr(&kernel_page_tables[pte_index]) | PAGE_DIR_KERNEL);
    }

    // Zero rest of pdes
    for (; pde_index < PTRS_PER_PMD; ++pde_index)
    {
        kernel_pde_set(pde_index, 0);
    }

    tlb_flush();

    ASSERT(kernel_page_tables[phys_addr(virt_end) / PAGE_SIZE - 1]);
    ASSERT(kernel_page_dir[min(last_phys_address, GiB) / PAGE_SIZE / PTRS_PER_PMD + KERNEL_PGD_OFFSET - 1]);
}
