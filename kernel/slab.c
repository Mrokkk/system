#include <kernel/debug.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/printk.h>
#include <kernel/page_alloc.h>

#define SLAB_ZERO_AFTER_FREE 1
#define SLAB_REDZONE        0
#define SLAB_POISON         0x5324
#define SLAB_REDZONE_POISON 0xffc0deff
#define SLAB_REDZONE_SIZE   8

typedef struct slab
{
    list_head_t list_entry;
    uint32_t    poison;
} slab_t;

typedef struct slab_allocator
{
    mutex_t     lock;
    size_t      size;
    page_t*     pages;
    list_head_t free;
} slab_allocator_t;

#define SLAB_32     0
#define SLAB_64     1
#define SLAB_128    2
#define SLAB_256    3
#define SLAB_512    4
#define SLABS_SIZE  5

static slab_allocator_t allocators[SLABS_SIZE];

static inline slab_allocator_t* slab_allocator_get(size_t size)
{
#if SLAB_REDZONE
    size += SLAB_REDZONE_SIZE;
#endif
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

    scoped_mutex_lock(&allocator->lock);

    if (unlikely(list_empty(&allocator->free)))
    {
        return NULL;
    }

    slab_t* slab = list_front(&allocator->free, slab_t, list_entry);
    list_del(&slab->list_entry);
    slab->poison = 0;

#if SLAB_REDZONE
    uint32_t* redzone = ptr(slab);
    *redzone = SLAB_REDZONE_POISON;
    redzone = ptr(addr(redzone) + size + 4);
    *redzone = SLAB_REDZONE_POISON;
    slab = ptr(addr(slab) + 4);
#endif

    return slab;
}

void slab_free(void* ptr, size_t size)
{
    slab_t* slab = ptr;
    slab_allocator_t* allocator = slab_allocator_get(size);

#if SLAB_REDZONE
    slab = ptr(addr(slab) - 4);
    uint32_t* redzone = ptr(slab);
    ASSERT(*redzone == SLAB_REDZONE_POISON);
    redzone = ptr(addr(redzone) + size + 4);
    ASSERT(*redzone == SLAB_REDZONE_POISON);
#endif

    // FIXME: this can be randomly happen on some data which happens to set exact same value
    if (unlikely(slab->poison == SLAB_POISON))
    {
        log_info("possibly freeing already free block %x", ptr);
    }

    if (unlikely(!allocator))
    {
        return;
    }

    list_init(&slab->list_entry);

    scoped_mutex_lock(&allocator->lock);

    list_add_tail(&slab->list_entry, &allocator->free);
    slab->poison = SLAB_POISON;

#if SLAB_ZERO_AFTER_FREE
    memset(shift(slab, sizeof(*slab)), 0, allocator->size - sizeof(*slab));
#endif
}

UNMAP_AFTER_INIT static void init(slab_allocator_t* allocator, size_t size, size_t count)
{
    uint8_t* ptr;
    page_t* pages;
    slab_t* slab;

    pages = page_alloc(page_align(count * size) / PAGE_SIZE, PAGE_ALLOC_CONT | PAGE_ALLOC_ZEROED);
    ptr = page_virt_ptr(pages);

    list_init(&allocator->free);
    mutex_init(&allocator->lock);
    allocator->pages = pages;
    allocator->size = size;

    for (size_t i = 0; i < count; ++i, ptr += size)
    {
        slab = ptr(ptr);
        list_init(&slab->list_entry);
        list_add_tail(&slab->list_entry, &allocator->free);
        slab->poison = SLAB_POISON;
    }
}

UNMAP_AFTER_INIT void slab_allocator_init(void)
{
    init(&allocators[SLAB_32], 32, 512 * 2);
    init(&allocators[SLAB_64], 64, 256 * 16);
    init(&allocators[SLAB_128], 128, 128 * 2);
    init(&allocators[SLAB_256], 256, 64 * 2);
    init(&allocators[SLAB_512], 512, 32 * 2);
}
