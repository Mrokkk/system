#define log_fmt(fmt) "page: " fmt
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/sections.h>
#include <kernel/page_debug.h>
#include <kernel/page_table.h>
#include <kernel/page_types.h>

uintptr_t last_pfn;
page_t* page_map;
extern list_head_t free_pages;

static inline void page_set_used(uintptr_t pfn)
{
    page_map[pfn].refcount = 1;
    page_map[pfn].virtual = virt_ptr(pfn * PAGE_SIZE);
    list_init(&page_map[pfn].list_entry);
}

static inline void page_set_unused(uintptr_t pfn)
{
    page_map[pfn].refcount = 0;
    page_map[pfn].virtual = NULL;
    list_add_tail(&page_map[pfn].list_entry, &free_pages);
}

#define USED 1
#define FREE 2

static inline void pages_set(uintptr_t start, uintptr_t end, int used)
{
    for (uint32_t pfn = start / PAGE_SIZE; pfn < end / PAGE_SIZE; ++pfn)
    {
        switch (used)
        {
            case USED: page_set_used(pfn); break;
            case FREE: page_set_unused(pfn); break;
        }
    }
}

static inline void page_map_init(uintptr_t virt_end)
{
    uint32_t start, end;
    uint32_t phys_end = phys_addr(virt_end);
    section_t* section = sections;

    // Low memory should be marked as used, as it is necessary for DMA, BIOS, etc
    pages_set(0, end = section_phys_start(section), USED);

    for (; section->name; ++section)
    {
        pages_set(end, start = section_phys_start(section), FREE);
        pages_set(start, end = section_phys_end(section), USED);
    }

    pages_set(end, phys_end, USED);
    pages_set(phys_end, last_phys_address, FREE);
}

static inline uintptr_t virt_end_get(void)
{
    uintptr_t end = 0;
    for (section_t* section = sections; section->name; ++section)
    {
        end = page_align(addr(section->end));
    }
    return end;
}

UNMAP_AFTER_INIT void paging_init(void)
{
    extern size_t page_tables_prealloc(uintptr_t virt_end);
    extern void page_tables_init(uintptr_t virt_end);

    last_pfn = last_phys_address / PAGE_SIZE;

    uintptr_t virt_end = virt_end_get();
    size_t page_map_size = page_align(last_pfn * sizeof(*page_map));
    size_t page_table_size = page_tables_prealloc(virt_end);

    log_notice("page map size = %u B (%u kiB, %u pages; %u entries, %u B each)",
        page_map_size,
        page_map_size / KiB,
        page_map_size / PAGE_SIZE,
        last_pfn,
        sizeof(page_t));

    page_map = ptr(virt_end + page_table_size);

    virt_end += page_table_size + page_map_size;

    page_tables_init(virt_end);
    page_map_init(virt_end);

    tlb_flush();

    ASSERT(!page_map[phys_addr(virt_end + PAGE_SIZE) / PAGE_SIZE].refcount);
    ASSERT(!page_map[phys_addr(virt_end) / PAGE_SIZE].refcount);
    ASSERT(page_map[(phys_addr(virt_end) - PAGE_SIZE) / PAGE_SIZE].refcount);
    ASSERT(page_map[memory_areas->start / PAGE_SIZE + 2].refcount);
}
