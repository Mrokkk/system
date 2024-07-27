#define log_fmt(fmt) "page: " fmt
#include <kernel/list.h>
#include <kernel/page.h>
#include <kernel/ksyms.h>
#include <kernel/mutex.h>
#include <kernel/memory.h>
#include <kernel/minmax.h>
#include <kernel/process.h>

#include <arch/segment.h>
#include <arch/register.h>
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

static vm_region_t* regions;
static vm_region_t* last_region;

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

    for (uint32_t i = 0; i < last_pfn; ++i)
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

static inline void page_kernel_identity_mapping(page_t* page, int pte_flag)
{
    uint32_t paddr = page_phys(page);
    uint32_t vaddr = virt(paddr);
    uint32_t pfn = paddr / PAGE_SIZE;

    pte_set(pfn, paddr | PTE_PRESENT | PTE_WRITEABLE | pte_flag);
    page->virtual = ptr(vaddr);
}

static inline void page_kernel_identity_mapping_range(page_t* page, int pte_flag)
{
    page_t* temp;
    page_kernel_identity_mapping(page, pte_flag);
    list_for_each_entry(temp, &page->list_entry, list_entry)
    {
        page_kernel_identity_mapping(temp, pte_flag);
    }
}

static inline int frame_free(uint32_t address)
{
    uint32_t frame = address / PAGE_SIZE;
    int ret;

    log_debug(DEBUG_PAGE, "[free] paddr=%x count_after_free=%u", address, page_map[frame].refcount - 1);

    if (unlikely(!page_map[frame].refcount))
    {
        panic("frame is already free or reserved: %x", address);
    }

    if (!(ret = --page_map[frame].refcount))
    {
        list_del(&page_map[frame].list_entry);
        list_add(&page_map[frame].list_entry, &free_pages);
    }

    return ret;
}

page_t* __page_alloc(int count, alloc_flag_t flag)
{
    page_t* first_page;

    ASSERT(count);

    mutex_lock(&page_mutex);

    first_page = flag & PAGE_ALLOC_CONT
        ? free_page_range_find(count)
        : free_page_range_find_discont(count);

    if (unlikely(!first_page))
    {
        mutex_unlock(&page_mutex);
        return NULL;
    }

    first_page->pages_count = count;
    if (!(flag & PAGE_ALLOC_NO_KERNEL))
    {
        page_kernel_identity_mapping_range(first_page, flag & PAGE_ALLOC_UNCACHED ? PTE_CACHEDIS : 0);
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

    mutex_unlock(&page_mutex);

    return first_page;
}

int __page_free(void* ptr)
{
    uint32_t frame;

    mutex_lock(&page_mutex);

    frame = phys_addr(ptr) / PAGE_SIZE;

    if (frame_free(phys_addr(ptr)) == 0)
    {
        pte_set(frame, 0);
        invlpg(ptr);
    }

    mutex_unlock(&page_mutex);
    return 0;
}

int __pages_free(page_t* pages)
{
    int count = 0;
    page_t* temp_page;
    uintptr_t paddr;

    mutex_lock(&page_mutex);

    list_for_each_entry_safe(temp_page, &pages->list_entry, list_entry)
    {
        paddr = page_phys(temp_page);
        list_del(&temp_page->list_entry);
        frame_free(paddr);
    }

    paddr = page_phys(pages);
    ASSERT(list_empty(&pages->list_entry));
    frame_free(paddr);

    mutex_unlock(&page_mutex);

    return count;
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

static inline void* page_map_region(uint32_t paddr_start, uint32_t vaddr_start, uint32_t size)
{
    pgt_t* pgt;
    uint32_t pde_index, pte_index, vaddr, paddr;

    log_debug(DEBUG_PAGE, "mapping %x size=%x", paddr_start, size);

    for (paddr = paddr_start, vaddr = vaddr_start;
        paddr < paddr_start + size;
        paddr += PAGE_SIZE, vaddr += PAGE_SIZE)
    {
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        if (!kernel_page_dir[pde_index])
        {
            pgt = pgt_alloc();
            if (unlikely(!pgt))
            {
                log_warning("cannot allocate pgt for identity mapping");
                return 0;
            }

            pde_set(pde_index, phys_addr(pgt) | PAGE_KERNEL_FLAGS | PDE_CACHEDIS);
        }
        else
        {
            pgt = virt_ptr(kernel_page_dir[pde_index] & PAGE_ADDRESS);
        }

        pgt[pte_index] = paddr | PAGE_KERNEL_FLAGS | PTE_CACHEDIS;
    }

    pgd_reload();
    return ptr(vaddr_start);
}

void pages_unmap(page_t* pages)
{
    pgt_t* pgt;
    uint32_t pde_index;
    uint32_t pte_index;
    page_t* temp = pages;

    for (uint32_t vaddr = page_virt(temp);; )
    {
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);
        pgt = virt_ptr(kernel_page_dir[pde_index] & PAGE_ADDRESS);
        pgt[pte_index] = 0;
        invlpg(ptr(vaddr));
        temp = list_next_entry(&temp->list_entry, page_t, list_entry);
        if (temp == pages)
        {
            break;
        }
    }
}

void* region_map(uint32_t paddr, uint32_t size, const char* name)
{
    memory_area_t* area;
    vm_region_t* region;
    uint32_t vaddr;

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

    log_debug(DEBUG_PAGE, "mapping %x => %x; size = %x", paddr, vaddr, size);

    if (vaddr < area->start || vaddr + size > area->end)
    {
        panic("%x - %x outside of area [%08x - %08x]", vaddr, vaddr + size, addr(area->start), addr(area->end - 1));
    }

    region->start = vaddr;
    region->end = vaddr + size;
    region->name = name;
    region->paddr = paddr;
    region->area = area;

    if (!page_map_region(paddr, vaddr, size))
    {
        log_warning("mapping failed");
        return NULL;
    }

    last_region = region;

    return ptr(vaddr);
}

void clear_first_pde(void)
{
    kernel_page_dir[0] = 0;
}

void page_map_panic(uint32_t start, uint32_t end)
{
    kernel_page_dir[0] = kernel_page_dir[KERNEL_PDE_OFFSET];
    pgt_t* pgt = virt_ptr(kernel_page_dir[0] & PAGE_ADDRESS);
    for (uint32_t paddr = start; paddr < end; paddr += PAGE_SIZE)
    {
        pgt[paddr / PAGE_SIZE] = paddr | PTE_WRITEABLE | PTE_PRESENT;
    }
    pgd_load(kernel_page_dir);
}

void page_stats_print()
{
    log_info("memory stats:");

    uint32_t frames_used = 0;
    uint32_t frames_free = 0;
    uint32_t frames_unavailable = 0;
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

    log_info("last_pfn=%x", last_pfn);
    log_info("frames_used=%u (%u kB)", frames_used, frames_used * 4);
    log_info("frames_free=%u (%u kB)", frames_free, frames_free * 4);
    log_info("frames_unavailable=%u (%u kB)", frames_unavailable, frames_unavailable * 4);

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
            ksym_string(symbol, addr(caller));
            log_info("%- 5u %08x %s",
                page_map[i].refcount,
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

static inline void page_set_used(uint32_t pfn)
{
    page_map[pfn].refcount = 1;
    page_map[pfn].virtual = virt_ptr(pfn * PAGE_SIZE);
    list_init(&page_map[pfn].list_entry);
}

static inline void page_set_unused(uint32_t pfn)
{
    page_map[pfn].refcount = 0;
    page_map[pfn].virtual = NULL;
    list_add_tail(&page_map[pfn].list_entry, &free_pages);
}

static inline uint32_t section_phys_start(section_t* section)
{
    return section->flags & SECTION_UNPAGED
        ? addr(section->start)
        : phys_addr(section->start);
}

static inline uint32_t section_phys_end(section_t* section)
{
    return section->flags & SECTION_UNPAGED
        ? addr(section->end)
        : phys_addr(section->end);
}

#define USED 1
#define FREE 2
static inline void pages_set(uint32_t start, uint32_t end, int used)
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

static inline void page_map_init(uint32_t virt_end)
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
    pages_set(phys_end, ram, FREE);
}

static inline int section_page_flags(int flags)
{
    uint32_t page_flags = PTE_PRESENT;
    if (flags & SECTION_WRITE) page_flags |= PTE_WRITEABLE;
    return page_flags;
}

static inline void pgt_init(pgt_t* prev_pgt, uint32_t virt_end)
{
    uint32_t pte_index, pde_index, flags, start, end = 0;
    uint32_t pfn_end = phys_addr(virt_end) / PAGE_SIZE;
    section_t* section = sections;

    // Set up currently used page tables so it will cover required space to setup pages for 4G
    for (pte_index = phys_addr(_end) / PAGE_SIZE; pte_index < min(INIT_PGT_SIZE * PAGES_IN_PTE, pfn_end); ++pte_index)
    {
        prev_pgt[pte_index] = (pte_index * PAGE_SIZE) | PAGE_KERNEL_FLAGS;
    }

    for (pte_index = 0; section->name; ++section)
    {
        // Set up gap between sections as not present
        start = section_phys_start(section);
        for (uint32_t addr = end; addr < start; addr += PAGE_SIZE, ++pte_index)
        {
            pte_set(pte_index, 0);
        }

        end = section_phys_end(section);
        flags = section_page_flags(section->flags);

        // Set up section pages as present with proper protection
        for (uint32_t addr = start; addr < end; addr += PAGE_SIZE, ++pte_index)
        {
            pte_set(pte_index, pte_index * PAGE_SIZE | flags);
        }
    }

    for (; pte_index < pfn_end; ++pte_index)
    {
        pte_set(pte_index, pte_index * PAGE_SIZE | PAGE_KERNEL_FLAGS);
    }

    // Set up page directory
    for (pde_index = KERNEL_PDE_OFFSET, pte_index = 0;
        pde_index < min(align(ram / PAGE_SIZE, PAGES_IN_PTE) / PAGES_IN_PTE + KERNEL_PDE_OFFSET, PTE_IN_PDE);
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

static inline uint32_t virt_end_get(void)
{
    uint32_t end = 0;
    for (section_t* section = sections; section->name; ++section)
    {
        end = page_align(addr(section->end));
    }
    return end;
}

UNMAP_AFTER_INIT int paging_init()
{
    pgt_t* temp_pgt;
    uint32_t virt_end, pgt_size, page_map_size;

    last_pfn = ram / PAGE_SIZE;
    temp_pgt = virt_ptr(page0);
    kernel_page_dir = virt_ptr(page_dir);

    page_map_size = page_align(last_pfn * sizeof(page_t));
    pgt_size = page_align(min(GiB, page_align(ram)) / PAGES_IN_PTE);

    virt_end = virt_end_get();

    // page_table will be allocated at the virt_end; it will cover up to 256 continguous tables for mapping
    // min of {1GiB, available RAM} kernel space
    page_table = ptr(virt_end);

    // page_map is allocated just after page tables; it will cover each available physical page frame
    page_map = ptr(virt_end + pgt_size);

    log_notice("kernel page tables size = %u B (%u KiB; %u pages)",
        pgt_size,
        pgt_size / KiB,
        pgt_size / PAGE_SIZE);

    log_notice("page map size = %u B (%u kiB, %u pages; %u entries, %u B each)",
        page_map_size,
        page_map_size / KiB,
        page_map_size / PAGE_SIZE,
        last_pfn,
        sizeof(page_t));

    virt_end += page_map_size + pgt_size;

    pgt_init(temp_pgt, virt_end);
    page_map_init(virt_end);

    pgd_load(kernel_page_dir);

    ASSERT(!page_map[phys_addr(virt_end + PAGE_SIZE) / PAGE_SIZE].refcount);
    ASSERT(!page_map[phys_addr(virt_end) / PAGE_SIZE].refcount);
    ASSERT(page_map[(phys_addr(virt_end) - PAGE_SIZE) / PAGE_SIZE].refcount);
    ASSERT(page_table[phys_addr(virt_end) / PAGE_SIZE - 1]);
    ASSERT(kernel_page_dir[min(ram, GiB) / PAGE_SIZE / PTE_IN_PDE + KERNEL_PDE_OFFSET - 1]);
    ASSERT(page_map[memory_areas->start / PAGE_SIZE + 2].refcount);

    return 0;
}
