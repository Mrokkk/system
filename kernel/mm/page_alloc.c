#define log_fmt(fmt) "page: " fmt
#include <kernel/list.h>
#include <kernel/ksyms.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>
#include <kernel/page_alloc.h>
#include <kernel/page_debug.h>
#include <kernel/page_table.h>

extern uintptr_t last_pfn;
extern page_t* page_map;

static MUTEX_DECLARE(page_mutex);
LIST_DECLARE(free_pages);

static inline page_t* free_page_find(void)
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

        log_debug(DEBUG_PAGE, "[alloc] %p", ptr(page_phys(temp_page)));

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
        log_debug(DEBUG_PAGE, "[alloc] %#zx", page_phys(&first_page[i]));
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
    const bool zero = flag & PAGE_ALLOC_ZEROED;
    const pgprot_t pgprot = kernel_identity_pgprot(flag);

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
            if (zero)
            {
                memset(page_virt_ptr(temp), 0, PAGE_SIZE);
            }
        }
        page_kernel_map(first_page, pgprot);
        if (zero)
        {
            memset(page_virt_ptr(first_page), 0, PAGE_SIZE);
        }
    }

#if DEBUG_PAGE_DETAILED
    void* caller = __builtin_return_address(0);
    first_page->caller = caller;
    list_for_each_entry(temp, &first_page->list_entry, list_entry)
    {
        temp->caller = caller;
    }
#endif

    return first_page;
}

static void page_free(page_t* page)
{
    log_debug(DEBUG_PAGE, "[free] paddr=%#zx count_after_free=%u", page_phys(page), page->refcount - 1);

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

void pages_merge(page_t* new_pages, page_t* pages)
{
    list_add(&new_pages->list_entry, &pages->list_entry);
    new_pages->pages_count += pages->pages_count;
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
    log_info("frames_used=%zu (%zu kB)", frames_used, frames_used * 4);
    log_info("frames_free=%zu (%zu kB)", frames_free, frames_free * 4);
    log_info("frames_unavailable=%u (%zu kB)", frames_unavailable, frames_unavailable * 4);

#if DEBUG_PAGE_DETAILED
    char symbol[80];
    log_info("count paddr      symbol");
    for (uint32_t i = 0; i < last_pfn; ++i)
    {
        if (page_map[i].caller && page_map[i].refcount)
        {
            void* caller = page_map[i].caller;
            ksym_string(addr(caller), symbol, sizeof(symbol));
            log_info("%- 5u %#010lx %s",
                page_map[i].refcount,
                page_phys(&page_map[i]),
                symbol);
        }
    }
#endif
}
