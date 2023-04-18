#include <kernel/page.h>
#include <kernel/malloc.h>
#include <kernel/printk.h>

#define SLAB_POISON 0x5324

typedef struct slab
{
    struct list_head list_entry;
    uint32_t poison;
} slab_t;

typedef struct slab_allocator
{
    size_t size;
    page_t* pages;
    struct list_head free;
} slab_allocator_t;

#define SLAB_32     0
#define SLAB_64     1
#define SLAB_128    2
#define SLAB_256    3
#define SLAB_512    4
#define SLABS_SIZE  5

slab_allocator_t allocators[SLABS_SIZE];

static inline slab_allocator_t* slab_allocator_get(size_t size)
{
    if (size <= 32) return allocators + SLAB_32;
    else if (size <= 64) return allocators + SLAB_64;
    else if (size <= 128) return allocators + SLAB_128;
    else if (size <= 256) return allocators + SLAB_256;
    else if (size <= 512) return allocators + SLAB_512;
    else return NULL;
}

void* slab_alloc(size_t size)
{
    slab_allocator_t* allocator = slab_allocator_get(size);

    if (unlikely(!allocator))
    {
        return NULL;
    }

    if (unlikely(list_empty(&allocator->free)))
    {
        return NULL;
    }

    slab_t* slab = list_front(&allocator->free, struct slab, list_entry);
    list_del(&slab->list_entry);
    slab->poison = 0;

    return slab;
}

void slab_free(void* ptr, size_t size)
{
    slab_t* slab = ptr;
    slab_allocator_t* allocator = slab_allocator_get(size);

    if (unlikely(slab->poison == SLAB_POISON))
    {
        log_error("freeing already free block %x", ptr);
        return;
    }

    if (unlikely(!allocator))
    {
        return;
    }

    list_init(&slab->list_entry);
    list_add_tail(&slab->list_entry, &allocator->free);
    slab->poison = SLAB_POISON;
}

static void init(struct slab_allocator* allocator, size_t size, size_t count)
{
    uint8_t* ptr;
    page_t* pages;
    slab_t* slab;

    pages = page_alloc(page_align(count * size) / PAGE_SIZE, PAGE_ALLOC_CONT);
    ptr = page_virt_ptr(pages);

    list_init(&allocator->free);
    allocator->pages = pages;

    for (size_t i = 0; i < count; ++i, ptr += size)
    {
        slab = ptr(ptr);
        list_init(&slab->list_entry);
        list_add_tail(&slab->list_entry, &allocator->free);
        slab->poison = SLAB_POISON;
    }
}

void slab_allocator_init(void)
{
    init(&allocators[SLAB_32], 32, 512);
    init(&allocators[SLAB_64], 64, 256);
    init(&allocators[SLAB_128], 128, 128);
    init(&allocators[SLAB_256], 256, 64);
    init(&allocators[SLAB_512], 512, 32);
}
