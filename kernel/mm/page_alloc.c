#define log_fmt(fmt) "page: " fmt
#include <kernel/list.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>
#include <kernel/page_alloc.h>
#include <kernel/page_debug.h>
#include <kernel/page_table.h>

struct mmio_region
{
    const char*    name;
    memory_area_t* area;
    uintptr_t      start;
    uintptr_t      end;
    uintptr_t      paddr;
};

typedef struct mmio_region mmio_region_t;

static mmio_region_t* regions;
static mmio_region_t* last_region;

extern uintptr_t last_pfn;
extern page_t* page_map;

static MUTEX_DECLARE(page_mutex);
LIST_DECLARE(free_pages);

static inline page_t* free_page_find()
{
    if (unlikely(list_empty(&free_pages)))
    {
        log_debug(DEBUG_PAGE, "no free pages!");
        return NULL;
    }

    page_t* page = list_front(&free_pages, page_t, list_entry);

    list_del(&page->list_entry);

    return page;
}

static inline page_t* free_page_range_find_discont(const int count)
{
    page_t* temp_page;
    page_t* first_page = NULL;

    for (int i = 0; i < count; ++i)
    {
        temp_page = free_page_find();

        if (unlikely(!temp_page))
        {
            goto no_pages;
        }

        log_debug(DEBUG_PAGE, "[alloc] %x", page_phys(temp_page));

        list_del(&temp_page->list_entry);
        temp_page->refcount = 1;

        if (!first_page)
        {
            first_page = temp_page;
        }
        else
        {
            temp_page->pages_count = 0;
            list_add_tail(&temp_page->list_entry, &first_page->list_entry);
        }
    }

    return first_page;

no_pages:
    // TODO: free pages
    return NULL;
}

static inline page_t* free_page_range_find(const int count)
{
    int temp_count = count;
    page_t* first_page = NULL;

    for (uintptr_t i = 0; i < last_pfn; ++i)
    {
        if (!page_map[i].refcount)
        {
            if (!first_page)
            {
                first_page = &page_map[i];
            }
            --temp_count;
        }
        else
        {
            first_page = NULL;
            temp_count = count;
        }
        if (!temp_count)
        {
            break;
        }
    }

    if (unlikely(!first_page))
    {
        return NULL;
    }

    for (int i = 0; i < count; ++i)
    {
        log_debug(DEBUG_PAGE, "[alloc] %x", page_phys(&first_page[i]));
        first_page[i].refcount = 1;
        first_page[i].pages_count = 0;
        list_del(&first_page[i].list_entry);

        if (i != 0)
        {
            list_add_tail(&first_page[i].list_entry, &first_page->list_entry);
        }
    }

    return first_page;
}

page_t* __page_alloc(int count, alloc_flag_t flag)
{
    page_t* temp;
    page_t* first_page;
    pgprot_t pgprot = kernel_identity_pgprot(flag);

    ASSERT(count);

    scoped_mutex_lock(&page_mutex);

    first_page = flag & PAGE_ALLOC_CONT
        ? free_page_range_find(count)
        : free_page_range_find_discont(count);

    if (unlikely(!first_page))
    {
        return NULL;
    }

    first_page->pages_count = count;
    if (!(flag & PAGE_ALLOC_NO_KERNEL))
    {
        list_for_each_entry(temp, &first_page->list_entry, list_entry)
        {
            page_kernel_map(temp, pgprot);
        }
        page_kernel_map(first_page, pgprot);
    }

#if DEBUG_PAGE_DETAILED
    void* caller = __builtin_return_address(0);
    first_page->caller = caller;
    page_t* temp;
    list_for_each_entry(temp, &first_page->list_entry, list_entry)
    {
        temp->caller = caller;
    }
#endif

    return first_page;
}

static void page_free(page_t* page)
{
    log_debug(DEBUG_PAGE, "[free] paddr=%x count_after_free=%u", page_phys(page), page->refcount - 1);

    if (unlikely(!page->refcount))
    {
        panic("page is already free or reserved: %p", page_phys(page));
    }

    if (!(--page->refcount))
    {
        if (page->virtual)
        {
            page_kernel_unmap(page);
        }

        list_del(&page->list_entry);
        list_add(&page->list_entry, &free_pages);
    }
}

void __pages_free(page_t* pages)
{
    page_t* temp_page;

    scoped_mutex_lock(&page_mutex);

    list_for_each_entry_safe(temp_page, &pages->list_entry, list_entry)
    {
        page_free(temp_page);
    }

    page_free(pages);
}

page_t* pages_split(page_t* pages, size_t pages_to_split)
{
    page_t* other_pages = NULL;
    page_t* removed;
    size_t count = 0;

    if (pages_to_split == pages->pages_count)
    {
        return pages;
    }

    do
    {
        count++;
        removed = list_prev_entry(&pages->list_entry, page_t, list_entry);
        list_del(&removed->list_entry);
        if (!other_pages)
        {
            other_pages = removed;
        }
        else
        {
            list_add(&removed->list_entry, &other_pages->list_entry);
        }
    }
    while (--pages_to_split);

    other_pages->pages_count = count;
    pages->pages_count -= count;

    return other_pages;
}

static inline void* page_mmio_map(uintptr_t paddr_start, uintptr_t vaddr_start, size_t size)
{
    uintptr_t vaddr, paddr;

    log_debug(DEBUG_PAGE, "mapping %x size=%x", paddr_start, size);

    for (paddr = paddr_start, vaddr = vaddr_start;
        paddr < paddr_start + size;
        paddr += PAGE_SIZE, vaddr += PAGE_SIZE)
    {
        pgd_t* pgde = pgd_offset(kernel_page_dir, vaddr);
        pud_t* pude = pud_alloc(pgde, vaddr);

        if (unlikely(!pude))
        {
            return NULL;
        }

        pmd_t* pmde = pmd_alloc(pude, vaddr);

        if (unlikely(!pmde))
        {
            return NULL;
        }

        pte_t* pte = pte_alloc(pmde, vaddr);

        if (unlikely(!pte))
        {
            return NULL;
        }

        pte_entry_set(pte, paddr, PAGE_MMIO);
    }

    return ptr(vaddr_start);
}

void* mmio_map(uintptr_t paddr, uintptr_t size, const char* name)
{
    memory_area_t* area;
    mmio_region_t* region;
    uintptr_t vaddr;

    size = page_align(size);

    if (!regions)
    {
        for (int i = MEMORY_AREAS_SIZE - 1; i; --i)
        {
            area = memory_areas + i;
            if (area->start < ~0UL && area->type && area->type != MMAP_TYPE_AVL)
            {
                break;
            }
        }
        region = regions = single_page();
        if (!regions)
        {
            return NULL;
        }
        memset(region, 0, PAGE_SIZE);
        vaddr = addr(area->end - size);
    }
    else
    {
        vaddr = last_region->start - size;
        area = last_region->area;
        region = last_region + 1;
    }

    log_debug(DEBUG_PAGE, "mapping %p => %p; size = %lx", paddr, vaddr, size);

    if (vaddr < area->start || vaddr + size > area->end)
    {
        panic("%p - %p outside of area [%p - %p]", vaddr, vaddr + size, addr(area->start), addr(area->end - 1));
    }

    region->start = vaddr;
    region->end = vaddr + size;
    region->name = name;
    region->paddr = paddr;
    region->area = area;

    if (!page_mmio_map(paddr, vaddr, size))
    {
        log_warning("mapping failed");
        return NULL;
    }

    last_region = region;

    return ptr(vaddr);
}

void page_stats_print()
{
    log_info("memory stats:");

    size_t frames_used = 0;
    size_t frames_free = 0;
    size_t frames_unavailable = 0;
    memory_area_t* ma;

    for (uint32_t i = 0; i < 10; ++i)
    {
        ma = &memory_areas[i];
        if (ma->type != MMAP_TYPE_AVL && ma->type != MMAP_TYPE_LOW)
        {
            frames_unavailable += (ma->end - ma->start) / PAGE_SIZE;
            continue;
        }

        for (uint32_t j = ma->start; j < ma->end; j += PAGE_SIZE)
        {
            if (ma->start > 0xffffffff) break;
            if (!page_map[j / PAGE_SIZE].refcount)
            {
                ++frames_free;
            }
            else
            {
                ++frames_used;
            }
        }
    }

    log_info("last_pfn=%p", ptr(last_pfn));
    log_info("frames_used=%zu (%u kB)", frames_used, frames_used * 4);
    log_info("frames_free=%zu (%u kB)", frames_free, frames_free * 4);
    log_info("frames_unavailable=%u (%zu kB)", frames_unavailable, frames_unavailable * 4);

    if (regions)
    {
        for (int i = 0;; ++i)
        {
            if (!regions[i].start)
            {
                break;
            }
            log_info("region: [%x - %x] paddr: %x name: %s",
                regions[i].start,
                regions[i].end - 1,
                regions[i].paddr,
                regions[i].name);
        }
    }

#if DEBUG_PAGE_DETAILED
    char symbol[80];
    log_info("count paddr      symbol");
    for (uint32_t i = 0; i < last_pfn; ++i)
    {
        if (page_map[i].caller && page_map[i].refcount)
        {
            void* caller = page_map[i].caller;
            ksym_string(addr(caller), symbol, sizeof(symbol));
            log_info("%- 5u %08x %s",
                page_map[i].refcount,
                page_phys(&page_map[i]),
                symbol);
        }
    }
#endif
}


