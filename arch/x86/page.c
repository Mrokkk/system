#include <kernel/ksyms.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>

#include <arch/page.h>
#include <arch/segment.h>
#include <arch/multiboot.h>
#include <arch/descriptor.h>

extern pgd_t page_dir[];
extern pgt_t page0[];

MUTEX_DECLARE(page_mutex);
LIST_DECLARE(free_pages);
uint32_t virt_end;
pgt_t* page_table;
pgd_t* kernel_page_dir;
page_t* page_map;

static uint32_t last_frame;

static struct page_stats
{
    size_t page_alloc_calls;
    size_t page_free_calls;
} page_stats;

static inline void pte_set(int nr, uint32_t val)
{
    page_table[nr] = val;
}

static inline void pge_set(int nr, uint32_t val)
{
    kernel_page_dir[nr] = val;
}

static inline page_t* free_page_find()
{
    flags_t flags;
    for (uint32_t i = 0; i < last_frame; ++i)
    {
        irq_save(flags);
        if (!page_map[i].count)
        {
            irq_restore(flags);
            return &page_map[i];
        }
        irq_restore(flags);
    }
    log_exception("no free page!");
    return NULL;
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

#if TRACE_PAGE
        log_debug("[alloc] %x", page_phys(temp_page));
#endif
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
    for (uint32_t i = 0; i < last_frame; ++i)
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
#if TRACE_PAGE
        log_debug("[alloc] %x", page_phys(&first_page[i]));
#endif
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

#if TRACE_PAGE_DETAILED
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

static inline int __frame_free(uint32_t address)
{
    uint32_t frame = address / PAGE_SIZE;

    log_debug("[free] paddr=%x count_after_free=%u", address, page_map[frame].count - 1);

    if (unlikely(!page_map[frame].count))
    {
        panic("frame is already free or reserved: %x", address);
    }
    return --page_map[frame].count;
}

int __page_free(void* ptr)
{
    mutex_lock(&page_mutex);

    ++page_stats.page_free_calls;
    uint32_t frame = phys_addr(ptr) / PAGE_SIZE;

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
        log_debug("paddr=%x vaddr=%x is used somewhere!", paddr, page_virt(first_page));
        list_init(&first_page->list_entry);
    }

    for (int i = 1; i < count; ++i)
    {
        temp = first_page + i;
        list_init(&temp->list_entry);
        list_add(&temp->list_entry, &first_page->list_entry);
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

pgd_t* pgd_alloc(pgd_t* old_pgd)
{
    pgd_t* new_pgd = single_page();

    if (unlikely(!new_pgd))
    {
        return NULL;
    }

    memset(new_pgd, 0, PAGE_SIZE);

    log_debug("copying %u kernel pte from %u", PTE_IN_PDE - KERNEL_PDE_OFFSET, KERNEL_PDE_OFFSET);

    memcpy(
        new_pgd + KERNEL_PDE_OFFSET,
        old_pgd + KERNEL_PDE_OFFSET,
        sizeof(uint32_t) * (PTE_IN_PDE - KERNEL_PDE_OFFSET));

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
    log_debug("mapping %x size=%x", paddr, size);
    for (uint32_t temp = paddr; temp < paddr + size; temp += PAGE_SIZE)
    {
        //log_debug("mapping %x", temp);
        uint32_t pde_index = pde_index(temp);
        uint32_t pte_index = pte_index(temp);
        pgt_t* pgt;

        if (!kernel_page_dir[pde_index])
        {
            pgt = pgt_alloc();
            if (unlikely(!pgt))
            {
                log_exception("cannot allocate pgt for identity mapping");
                return;
            }

            //log_debug("creating pgt at %u", pde_index);
            kernel_page_dir[pde_index] = phys_addr(pgt) | PAGE_KERNEL_FLAGS;
        }
        else
        {
            //log_debug("entry already exists at %u", pde_index);
            pgt = virt_ptr(kernel_page_dir[pde_index] & PAGE_ADDRESS);
        }

        pgt[pte_index] = temp | PAGE_KERNEL_FLAGS;
    }
    pgd_reload();
}

void page_stats_print()
{
    log_debug("memory stats:");

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

    log_debug("virt_end=%x", virt_end);
    log_debug("module_start=%x", module_start);
    log_debug("frames_used=%u (%u kB)", frames_used, frames_used * 4);
    log_debug("frames_free=%u (%u kB)", frames_free, frames_free * 4);
    log_debug("frames_lost=%u (%u kB)", frames_lost, frames_lost * 4);
    log_debug("frames_unavailable=%u (%u kB)", frames_unavailable, frames_unavailable * 4);
    log_debug("page_alloc_calls=%u", page_stats.page_alloc_calls);
    log_debug("page_free_calls=%u", page_stats.page_free_calls);

#if TRACE_PAGE_DETAILED
    char symbol[80];
    log_debug("count paddr      symbol");
    for (uint32_t i = 0; i < last_frame; ++i)
    {
        if (page_map[i].caller && page_map[i].count)
        {
            void* caller = page_map[i].caller;
            ksym_string(symbol, addr(caller));
            log_debug("%- 5u %08x %s",
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

uint32_t page_map_allocate(uint32_t virt_end)
{
    page_map = ptr(page_align(virt_end));
    uint32_t needed_pages = last_frame;
    uint32_t offset = addr(page_map) - virt_end + page_align(needed_pages * sizeof(page_t));

    log_debug("additional %u kB for metadata (%u entries; %u B each); virt_end=%x; page_map=%x",
        offset / 1024,
        needed_pages,
        sizeof(page_t),
        virt_end,
        page_map);

    return offset;
}

static inline uint32_t min(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static inline uint32_t pgt_init(pgt_t* prev_pgt, const uint32_t virt_end)
{
    page_table = ptr(virt_end);

    uint32_t address = phys(virt_end);;
    uint32_t frame_nr = address / PAGE_SIZE;
    uint32_t pages_for_pgt = min(1024 * 1024 * 1024, page_align(ram)) / PAGE_SIZE / 1024 + 1;

    log_debug("%u pages needed for kernel page tables", pages_for_pgt);

    prev_pgt[frame_nr] = address | PTE_PRESENT | PTE_WRITEABLE;

    for (uint32_t i = phys_addr(_end) / PAGE_SIZE; i < phys_addr(virt_end) / PAGE_SIZE + pages_for_pgt; ++i)
    {
        prev_pgt[i] = (i * PAGE_SIZE) | PAGE_KERNEL_FLAGS;
    }

    memcpy(page_table, prev_pgt, PAGE_SIZE);

    for (uint32_t i = KERNEL_PDE_OFFSET, j = 0; i < KERNEL_PDE_OFFSET + pages_for_pgt; i++, j += PTE_IN_PDE)
    {
        pge_set(i, phys_addr(&page_table[j]) | PDE_PRESENT | PDE_WRITEABLE);
    }

    for (uint32_t i = KERNEL_PDE_OFFSET + pages_for_pgt; i < PTE_IN_PDE; ++i)
    {
        pge_set(i, 0);
    }

    for (frame_nr = 0; frame_nr < phys_addr(virt_end + pages_for_pgt * PAGE_SIZE) / PAGE_SIZE; ++frame_nr)
    {
        page_map[frame_nr].count = 1;
        page_map[frame_nr].virtual = virt_ptr(frame_nr * PAGE_SIZE);
        list_init(&page_map[frame_nr].list_entry);
    }

    // Mark rest of pages as free
    for (; frame_nr < last_frame; ++frame_nr)
    {
        page_map[frame_nr].count = 0;
        page_map[frame_nr].virtual = NULL;
        list_add_tail(&page_map[frame_nr].list_entry, &free_pages);
    }

    log_debug("frame nr = %u", frame_nr);

    return pages_for_pgt * PAGE_SIZE;
}

static inline uint32_t virt_end_init()
{
    if (module_end > addr(_end))
    {
        // FIXME: Grub leaves a blank space between kernel end and first module;
        // mark frames as used only for selected region
        log_debug("changing end ptr to %x from %x (gap %u kB)", module_end, _end, (module_start - addr(_end)) / 1024);
        return module_end;
    }
    else
    {
        return addr(_end);
    }
}

int paging_init()
{
    last_frame = ram / PAGE_SIZE;
    pgt_t* temp_pgt = virt_ptr(page0);
    kernel_page_dir = virt_ptr(page_dir);

    virt_end = virt_end_init();
    virt_end += page_map_allocate(virt_end);
    virt_end += pgt_init(temp_pgt, virt_end);

    log_debug("virt_end=%x, need %u frames", virt_end, phys_addr(virt_end) / PAGE_SIZE);

    // nullptr page
    page_dir[0] = 0;

    ASSERT(!page_map[phys_addr(virt_end + PAGE_SIZE) / PAGE_SIZE].count);
    ASSERT(!page_map[phys_addr(virt_end) / PAGE_SIZE].count);
    ASSERT(page_map[(phys_addr(virt_end) - PAGE_SIZE) / PAGE_SIZE].count);
    ASSERT(page_table[phys_addr(virt_end) / PAGE_SIZE - 1]);
    ASSERT(!page_table[phys_addr(virt_end) / PAGE_SIZE]);

    pgd_load(phys_ptr(kernel_page_dir));

    return 0;
}
