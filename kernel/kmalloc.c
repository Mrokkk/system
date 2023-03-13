#include <kernel/ksyms.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>

#include <arch/page.h>

#define BLOCK_FREE 1
#define BLOCK_BUSY 0

SPINLOCK_DECLARE(kmalloc_lock);
LIST_DECLARE(kmalloc_pages);

typedef struct memory_block
{
    union
    {
        struct
        {
            uint16_t size;
            char free;
            struct list_head blocks;
#if TRACE_KMALLOC
            void* caller;
#endif
        };
        struct
        {
            uint8_t dummy[MEMORY_BLOCK_SIZE];
            uint32_t block_ptr[0];
        };
    };
} memory_block_t;

typedef struct alloc_data
{
    void* caller;
    size_t used;
} alloc_data_t;

static struct kmalloc_stats
{
    size_t kmalloc_calls;
    size_t kfree_calls;
} kmalloc_stats;

extern size_t ram;
static char* heap;
LIST_DECLARE(memory_blocks);

static inline void* ksbrk(uint32_t incr)
{
    void* prev_heap;
    uint32_t last_page, new_page;
    size_t diff;
    static struct memory_block* temp;
    page_t* page_range;

    prev_heap = heap;

    last_page = addr(prev_heap) / PAGE_SIZE;
    new_page = (addr(prev_heap) + incr) / PAGE_SIZE;

    diff = new_page - last_page;

    if (diff)
    {
        // FIXME: rewrite this
        size_t size = new_page * PAGE_SIZE - addr(prev_heap);
        if (size > MEMORY_BLOCK_SIZE)
        {
            temp = prev_heap;
            temp->free = 1;
            temp->size = size - MEMORY_BLOCK_SIZE;
            list_init(&temp->blocks);
            list_add_tail(&temp->blocks, &memory_blocks);
        }

        page_range = page_alloc(diff, PAGE_ALLOC_CONT);

        if (unlikely(!page_range))
        {
            return NULL;
        }

        list_merge(&kmalloc_pages, &page_range->list_entry);
        prev_heap = heap = page_virt_ptr(page_range);
    }

    heap += incr;

    return prev_heap;
}

static memory_block_t* kmalloc_create_block(size_t size)
{
    struct memory_block* new;

    if (!(new = (memory_block_t*)ksbrk(MEMORY_BLOCK_SIZE + size)))
    {
        return new; // We're out of memory
    }
    new->size = size;
    new->free = 0;
    list_init(&new->blocks);

    return new;
}

// FIXME: kmalloc has some crash after a sequence of 1024 allocations when compiling with -O2
void* kmalloc(size_t size)
{
    struct memory_block* temp;
    struct memory_block* new;

    spinlock_lock(&kmalloc_lock);

    ++kmalloc_stats.kmalloc_calls;

    // Align size to multiplication of MEMORY_BLOCK_SIZE
    if (size & MEMORY_BLOCK_SIZE)
    {
        size = (size & ~MEMORY_BLOCK_SIZE) + MEMORY_BLOCK_SIZE;
    }

    list_for_each_entry(temp, &memory_blocks, blocks)
    {
        // If we have found block with bigger size
        if (temp->free && temp->size >= size)
        {
            size_t old_size = temp->size;

            // Try to divide it
            if (old_size <= size + 2 * MEMORY_BLOCK_SIZE)
            {
                temp->free = 0;
            }
            else
            {
                temp->free = 0;
                temp->size = size;
                new = (memory_block_t*)(addr(temp->block_ptr) + size);
                new->size = old_size - MEMORY_BLOCK_SIZE - temp->size;
                new->free = 1;
                list_add(&new->blocks, &temp->blocks);
            }

#if TRACE_KMALLOC
            temp->caller = __builtin_return_address(0);
#endif

            spinlock_unlock(&kmalloc_lock);
            return ptr(temp->block_ptr);
        }
    }

    // Add next block to the end of the list
    if (!(new = kmalloc_create_block(size)))
    {
        spinlock_unlock(&kmalloc_lock);
        return 0;
    }
    list_add_tail(&new->blocks, &memory_blocks);
#if TRACE_KMALLOC
    new->caller = __builtin_return_address(0);
#endif

    spinlock_unlock(&kmalloc_lock);
    return (void*)new->block_ptr;
}

int kfree(void* ptr)
{
    struct memory_block* temp;

    spinlock_lock(&kmalloc_lock);
    ++kmalloc_stats.kfree_calls;

    list_for_each_entry(temp, &memory_blocks, blocks)
    {
        if (addr(temp->block_ptr) == addr(ptr))
        {
            temp->free = 1;
#if TRACE_KMALLOC
            temp->caller = NULL;
#endif
            spinlock_unlock(&kmalloc_lock);
            return 0;
        }
    }

    spinlock_unlock(&kmalloc_lock);
    log_debug("cannot free %x", ptr);

    return -ENXIO;
}

void kmalloc_init(void)
{
    page_t* page_range = page_alloc(1, PAGE_ALLOC_DISCONT);

    if (unlikely(!page_range))
    {
        panic("cannot allocate initial page for kmalloc!");
    }

    list_add_tail(&page_range->list_entry, &kmalloc_pages);
    heap = page_virt_ptr(page_range);
}

void kmalloc_stats_print(void)
{
    struct memory_block* temp;
    unsigned free = 0;
    unsigned used = 0;
    size_t i;
#if TRACE_KMALLOC
    alloc_data_t* alloc_data = single_page();
    __builtin_memset(alloc_data, 0, PAGE_SIZE);
    size_t max_entries = PAGE_SIZE / sizeof(alloc_data) - 1;
#endif

    list_for_each_entry(temp, &memory_blocks, blocks)
    {
        if (temp->free)
        {
            free += temp->size;
        }
        else
        {
#if TRACE_KMALLOC
            for (i = 0; i < max_entries; ++i)
            {
                if (alloc_data[i].caller == temp->caller)
                {
                    alloc_data[i].used += temp->size;
                    break;
                }
                else if (alloc_data[i].caller == NULL && alloc_data[i].used == 0)
                {
                    alloc_data[i].caller = temp->caller;
                    alloc_data[i].used = temp->size;
                    break;
                }
            }
#endif
            used += temp->size;
        }
    }

#if TRACE_KMALLOC
    char symbol[80];
    log_debug("used[B]  symbol");
    for (i = 0; i < max_entries; ++i)
    {
        void* caller = alloc_data[i].caller;
        size_t size = alloc_data[i].used;

        if (!caller)
        {
            break;
        }

        ksym_string(symbol, addr(caller));
        log_debug("%- 8u %s", size, symbol);
    }
    page_free(alloc_data);
#endif

    i = 0;
    page_t* page;
    list_for_each_entry(page, &kmalloc_pages, list_entry)
    {
        ++i;
    }

    log_debug("free=%u (%u kB)", free, free / 1024);
    log_debug("used=%u (%u kB)", used, used / 1024);
    log_debug("used_pages=%u", i);
    log_debug("kmalloc_calls=%u", kmalloc_stats.kmalloc_calls);
    log_debug("kfree_calls=%u", kmalloc_stats.kfree_calls);
}
