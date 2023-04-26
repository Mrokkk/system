#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>
#include <kernel/minmax.h>

#include <arch/segment.h>
#include <arch/multiboot.h>
#include <arch/descriptor.h>

extern pgd_t page_dir[];
extern pgt_t page0[];

static MUTEX_DECLARE(page_mutex);
static LIST_DECLARE(free_pages);

page_t* page_map;
static pgt_t* page_table;
static pgd_t* kernel_page_dir;
static uint32_t last_pfn;

static struct page_stats
{
    size_t page_alloc_calls;
    size_t page_free_calls;
} page_stats;

static inline void pte_set(int pte_index, uint32_t val)
{
    page_table[pte_index] = val;
}

static inline void pde_set(int pde_index, uint32_t val)
{
    kernel_page_dir[pde_index] = val;
}

static inline page_t* free_page_find()
{
    if (unlikely(list_empty(&free_pages)))
    {
        log_exception("no free page!");
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
        temp_page->count = 1;

        if (!first_page)
        {
            first_page = temp_page;
        }
        else
        {
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

    for (uint32_t i = 0; i < last_pfn; ++i)
    {
        if (!page_map[i].count)
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
        first_page[i].count = 1;
        list_del(&first_page[i].list_entry);
        if (i != 0)
        {
            list_add_tail(&first_page[i].list_entry, &first_page->list_entry);
        }
    }

    return first_page;
}

static inline void page_kernel_identity_mapping(page_t* page)
{
    uint32_t paddr = page_phys(page);
    uint32_t vaddr = virt(paddr);
    uint32_t pfn = paddr / PAGE_SIZE;

    pte_set(pfn, paddr | PTE_PRESENT | PTE_WRITEABLE);
    page->virtual = ptr(vaddr);
}

static inline void page_kernel_identity_mapping_range(page_t* page)
{
    page_t* temp;
    page_kernel_identity_mapping(page);
    list_for_each_entry(temp, &page->list_entry, list_entry)
    {
        page_kernel_identity_mapping(temp);
    }
}

static inline int frame_free(uint32_t address)
{
    uint32_t frame = address / PAGE_SIZE;

    log_debug(DEBUG_PAGE, "[free] paddr=%x count_after_free=%u", address, page_map[frame].count - 1);

    if (unlikely(!page_map[frame].count))
    {
        panic("frame is already free or reserved: %x", address);
    }

    list_del(&page_map[frame].list_entry);
    list_add(&page_map[frame].list_entry, &free_pages);

    return --page_map[frame].count;
}

page_t* __page_alloc(int count, int flags)
{
    page_t* first_page;

    if (unlikely(!count))
    {
        panic("page alloc called with 0 pages");
    }

    mutex_lock(&page_mutex);

    first_page = flags & PAGE_ALLOC_CONT
        ? free_page_range_find(count)
        : free_page_range_find_discont(count);

    if (unlikely(!first_page))
    {
        mutex_unlock(&page_mutex);
        return NULL;
    }

    page_kernel_identity_mapping_range(first_page);

#if DEBUG_PAGE_DETAILED
    void* caller = __builtin_return_address(0);
    first_page->caller = caller;
    page_t* temp;
    list_for_each_entry(temp, &first_page->list_entry, list_entry)
    {
        temp->caller = caller;
    }
#endif

    mutex_unlock(&page_mutex);

    return first_page;
}

int __page_free(void* ptr)
{
    uint32_t frame;

    mutex_lock(&page_mutex);

    ++page_stats.page_free_calls;
    frame = phys_addr(ptr) / PAGE_SIZE;

    if (frame_free(phys_addr(ptr)) == 0)
    {
        pte_set(frame, 0);
        invlpg(ptr);
    }

    mutex_unlock(&page_mutex);
    return 0;
}

// TODO: maybe this would not be needed, find better way for handling mbfs pages
page_t* __page_range_get(uint32_t paddr, int count)
{
    page_t* temp;
    page_t* first_page = page(paddr);

    mutex_lock(&page_mutex);

    if (!list_empty(&first_page->list_entry))
    {
        log_warning("paddr=%x vaddr=%x is used somewhere!", paddr, page_virt(first_page));
        list_init(&first_page->list_entry);
    }

    for (int i = 1; i < count; ++i)
    {
        temp = first_page + i;
        list_init(&temp->list_entry);
        list_add_tail(&temp->list_entry, &first_page->list_entry);
    }

    mutex_unlock(&page_mutex);

    return first_page;
}

int __page_range_free(struct list_head* head)
{
    int count = 0;
    struct list_head* temp;
    struct list_head* temp2;
    page_t* temp_page;

    if (unlikely(list_empty(head)))
    {
        panic("list is empty!");
    }

    mutex_lock(&page_mutex);

    page_t* first_page = list_front(head, page_t, list_entry);
    page_t* last_page = list_back(head, page_t, list_entry);

    for (temp = &first_page->list_entry;;)
    {
        ++count;
        temp_page = list_entry(temp, page_t, list_entry);

        temp2 = temp;
        temp = temp->next;
        list_del(temp2);

        frame_free(page_phys(temp_page));

        if (temp_page == last_page)
        {
            break;
        }
    }

    mutex_unlock(&page_mutex);

    return count;
}

pgd_t* pgd_alloc(void)
{
    pgd_t* new_pgd = single_page();

    if (unlikely(!new_pgd))
    {
        return NULL;
    }

    log_debug(DEBUG_PAGE, "copying %u kernel pte", PTE_IN_PDE);

    memcpy(
        new_pgd,
        kernel_page_dir,
        sizeof(uint32_t) * PTE_IN_PDE);

    return new_pgd;
}

pgt_t* pgt_alloc(void)
{
    pgt_t* new_pgt = single_page();

    if (unlikely(!new_pgt))
    {
        return NULL;
    }

    memset(new_pgt, 0, PAGE_SIZE);
    return new_pgt;
}

pgd_t* init_pgd_get(void)
{
    return kernel_page_dir;
}

void page_kernel_identity_map(uint32_t paddr, uint32_t size)
{
    pgt_t* pgt;
    uint32_t pde_index;
    uint32_t pte_index;

    log_debug(DEBUG_PAGE, "mapping %x size=%x", paddr, size);

    for (uint32_t temp = paddr; temp < paddr + size; temp += PAGE_SIZE)
    {
        pde_index = pde_index(temp);
        pte_index = pte_index(temp);

        if (!kernel_page_dir[pde_index])
        {
            pgt = pgt_alloc();
            if (unlikely(!pgt))
            {
                log_exception("cannot allocate pgt for identity mapping");
                return;
            }

            pde_set(pde_index, phys_addr(pgt) | PAGE_KERNEL_FLAGS);
        }
        else
        {
            pgt = virt_ptr(kernel_page_dir[pde_index] & PAGE_ADDRESS);
        }

        pgt[pte_index] = temp | PAGE_KERNEL_FLAGS;
    }
    pgd_reload();
}

void page_stats_print()
{
    log_info("memory stats:");

    uint32_t frames_used = 0;
    uint32_t frames_free = 0;
    uint32_t frames_lost = 0;
    uint32_t frames_unavailable = 0;
    memory_area_t* ma;

    for (uint32_t i = 0; i < 10; ++i)
    {
        ma = &memory_areas[i];
        if (ma->type != MMAP_TYPE_AVL)
        {
            frames_unavailable += ma->size / PAGE_SIZE;
            continue;
        }

        for (uint32_t j = ma->base; j < ma->base + ma->size; j += PAGE_SIZE)
        {
            if (!page_map[j / PAGE_SIZE].count)
            {
                ++frames_free;
            }
            else
            {
                virt(j) > addr(_end) && virt(j) < module_start
                    ? ++frames_lost
                    : ++frames_used;
            }
        }
    }

    log_info("last_pfn=%x", last_pfn);
    log_info("module_start=%x", module_start);
    log_info("frames_used=%u (%u kB)", frames_used, frames_used * 4);
    log_info("frames_free=%u (%u kB)", frames_free, frames_free * 4);
    log_info("frames_lost=%u (%u kB)", frames_lost, frames_lost * 4);
    log_info("frames_unavailable=%u (%u kB)", frames_unavailable, frames_unavailable * 4);
    log_info("page_alloc_calls=%u", page_stats.page_alloc_calls);
    log_info("page_free_calls=%u", page_stats.page_free_calls);

#if DEBUG_PAGE_DETAILED
    char symbol[80];
    log_info("count paddr      symbol");
    for (uint32_t i = 0; i < last_pfn; ++i)
    {
        if (page_map[i].caller && page_map[i].count)
        {
            void* caller = page_map[i].caller;
            ksym_string(symbol, addr(caller));
            log_info("%- 5u %08x %s",
                page_map[i].count,
                page_phys(&page_map[i]),
                symbol);
        }
    }
#endif
}

void pde_print(const uint32_t pde, char* output)
{
    output += sprintf(output, "pgt{phys_addr=%x, flags=(", pde & ~PAGE_MASK);
    output += sprintf(output, (pde & PDE_PRESENT)
        ? "present "
        : "non-present ");
    output += sprintf(output, (pde & PDE_WRITEABLE)
        ? "writable "
        : "non-writable ");
    output += sprintf(output, (pde & PDE_USER)
        ? "user)}"
        : "kernel)}");
}

void pte_print(const uint32_t pte, char* output)
{
    output += sprintf(output, "pg{phys_addr=%x, flags=(", pte & ~PAGE_MASK);
    output += sprintf(output, (pte & PTE_PRESENT)
        ? "present "
        : "non-present ");
    output += sprintf(output, (pte & PTE_WRITEABLE)
        ? "writable "
        : "non-writable ");
    output += sprintf(output, (pte & PTE_USER)
        ? "user)}"
        : "kernel)}");
}

static inline void page_map_init(uint32_t virt_end)
{
    uint32_t pfn;
    uint32_t pfn_end = phys_addr(virt_end) / PAGE_SIZE;

    // Mark pages as used
    for (pfn = 0; pfn < pfn_end; ++pfn)
    {
        page_map[pfn].count = 1;
        page_map[pfn].virtual = virt_ptr(pfn * PAGE_SIZE);
        list_init(&page_map[pfn].list_entry);
    }

    // Mark rest of pages as free
    for (; pfn < last_pfn; ++pfn)
    {
        page_map[pfn].count = 0;
        page_map[pfn].virtual = NULL;
        list_add_tail(&page_map[pfn].list_entry, &free_pages);
    }
}

static inline void pgt_init(pgt_t* prev_pgt, uint32_t virt_end)
{
    uint32_t pte_index, pde_index;
    uint32_t pfn_end = phys_addr(virt_end) / PAGE_SIZE;

    // Set up currently used page tables so it will cover required space to setup pages for 4G
    for (pte_index = phys_addr(_end) / PAGE_SIZE; pte_index < min(4 * PAGES_IN_PTE, pfn_end); ++pte_index)
    {
        prev_pgt[pte_index] = (pte_index * PAGE_SIZE) | PAGE_KERNEL_FLAGS;
    }

    // Set up new page tables; continguous page tables covering up to 1GiB of kernel space
    for (pte_index = 0; pte_index < pfn_end; ++pte_index)
    {
        pte_set(pte_index, pte_index * PAGE_SIZE | PAGE_KERNEL_FLAGS);
    }

    // Set up page directory
    for (pde_index = KERNEL_PDE_OFFSET, pte_index = 0;
        pde_index < ram / PAGE_SIZE / PAGES_IN_PTE + KERNEL_PDE_OFFSET;
        pde_index++, pte_index += PTE_IN_PDE)
    {
        pde_set(pde_index, phys_addr(&page_table[pte_index]) | PAGE_KERNEL_FLAGS);
    }

    // Zero rest of pdes
    for (; pde_index < PTE_IN_PDE; ++pde_index)
    {
        pde_set(pde_index, 0);
    }
}

static inline uint32_t virt_end_get(uint32_t* gap_start, uint32_t* gap_end)
{
    if (module_end > addr(_end))
    {
        // FIXME: Grub leaves a blank space between kernel end and first module;
        // mark frames as used only for selected region
        *gap_start = page_align(addr(_end));
        *gap_end = page_beginning(module_start);

        log_info("changing end ptr to %x from %x (gap %u kB)",
            page_align(module_end),
            _end,
            *gap_end - *gap_start);

        return page_align(module_end);
    }
    else
    {
        *gap_start = 0;
        *gap_end = 0;

        return page_align(addr(_end));
    }
}

int paging_init()
{
    pgt_t* temp_pgt;
    uint32_t virt_end, pgt_size, page_map_size, gap_start, gap_end, temp, gap_size;

    last_pfn = ram / PAGE_SIZE;
    temp_pgt = virt_ptr(page0);
    kernel_page_dir = virt_ptr(page_dir);

    page_map_size = page_align(last_pfn * sizeof(page_t));
    pgt_size = page_align(min(GiB, page_align(ram)) / PAGES_IN_PTE);

    virt_end = virt_end_get(&gap_start, &gap_end);

    // page_table will be allocated at the virt_end; it will cover up to 256 continguous tables for mapping
    // min of {1GiB, available RAM} kernel space
    page_table = ptr(virt_end);

    // page_map is allocated just after page tables; it will cover each available physical page frame
    page_map = ptr(virt_end + pgt_size);

    log_info("kernel page tables size = %u B (%u KiB; %u pages)",
        pgt_size,
        pgt_size / KiB,
        pgt_size / PAGE_SIZE);

    log_info("page map size = %u B (%u kiB, %u pages; %u entries, %u B each)",
        page_map_size,
        page_map_size / KiB,
        page_map_size / PAGE_SIZE,
        last_pfn,
        sizeof(page_t));

    virt_end += page_map_size + pgt_size;

    pgt_init(temp_pgt, virt_end);
    page_map_init(virt_end);

    kernel_page_dir[0] = 0;

    ASSERT(!page_map[phys_addr(virt_end + PAGE_SIZE) / PAGE_SIZE].count);
    ASSERT(!page_map[phys_addr(virt_end) / PAGE_SIZE].count);
    ASSERT(page_map[(phys_addr(virt_end) - PAGE_SIZE) / PAGE_SIZE].count);
    ASSERT(page_table[phys_addr(virt_end) / PAGE_SIZE - 1]);
    ASSERT(!page_table[phys_addr(virt_end) / PAGE_SIZE]);
    ASSERT(kernel_page_dir[ram / PAGE_SIZE / PTE_IN_PDE + KERNEL_PDE_OFFSET - 1]);
    ASSERT(!kernel_page_dir[ram / PAGE_SIZE / PTE_IN_PDE + KERNEL_PDE_OFFSET]);

    pgd_load(phys_ptr(kernel_page_dir));

    gap_size = gap_end - gap_start;

    log_info("freeing %u B (%u KiB; %u pages)",
        gap_size + 4 * PAGE_SIZE,
        (gap_size + 4 * PAGE_SIZE) / KiB,
        gap_size / PAGE_SIZE + 4);

    for (temp = gap_start; temp < gap_end; temp += PAGE_SIZE)
    {
        page_free(ptr(temp));
    }

    for (temp = virt_addr(page0); temp < virt_addr(page0) + 4 * PAGE_SIZE; temp += PAGE_SIZE)
    {
        page_free(ptr(temp));
    }

    return 0;
}
