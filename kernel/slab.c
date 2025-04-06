#include <kernel/debug.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/printk.h>
#include <kernel/compiler.h>
#include <kernel/page_alloc.h>

#define SLAB_CLASS(class_size, count) \
    SLAB_##class_size,

enum
{
#include "slab_classes.h"
};

#define SLAB_ZERO_AFTER_FREE 1
#define SLAB_POISON          0x02532401

struct slab_block
{
    list_head_t list_entry;
    uint32_t    poison;
};

struct slab
{
    page_t*     pages;
    uintptr_t   start;
    uintptr_t   end;
    size_t      allocated;
    list_head_t free;
    list_head_t list_entry;
};

struct slab_allocator
{
    mutex_t     lock;
    uint16_t    size;
    uint16_t    slabs_count;
    size_t      slab_size;
    list_head_t slabs;
};

typedef struct slab slab_t;
typedef struct slab_block slab_block_t;
typedef struct slab_allocator slab_allocator_t;

static MUTEX_DECLARE(lock);
static LIST_DECLARE(slabs_free);

#define SLAB_ENTRY_SIZE  (align(sizeof(slab_t), 32))
#define SLAB_ENTRY_COUNT 512

static_assert((PAGE_SIZE % SLAB_ENTRY_SIZE) == 0);

#undef SLAB_CLASS
#define SLAB_CLASS(class_size, count) \
    [SLAB_##class_size] = { \
        .lock      = MUTEX_INIT(allocators[SLAB_##class_size].lock), \
        .size      = class_size, \
        .slab_size = page_align((class_size) * (count)), \
        .slabs     = LIST_INIT(allocators[SLAB_##class_size].slabs) \
    },

static slab_allocator_t allocators[] = {
#include "slab_classes.h"
};

static inline slab_allocator_t* slab_allocator_get(size_t size)
{
#undef SLAB_CLASS
#define SLAB_CLASS(class_size, count) \
    if (size <= class_size) return allocators + SLAB_##class_size;
#include "slab_classes.h"
    return NULL;
}

static void* slab_entry_alloc(void)
{
    scoped_mutex_lock(&lock);

    if (list_empty(&slabs_free))
    {
        size_t size = page_align(SLAB_ENTRY_COUNT * SLAB_ENTRY_SIZE);
        page_t* pages = page_alloc(size / PAGE_SIZE, 0);
        page_t* page;

        PAGES_FOR_EACH(page, pages)
        {
            void* ptr = page_virt_ptr(page);
            for (size_t i = 0; i < PAGE_SIZE / SLAB_ENTRY_SIZE; ++i, ptr += SLAB_ENTRY_SIZE)
            {
                slab_block_t* block = ptr(ptr);
                list_init(&block->list_entry);
                list_add_tail(&block->list_entry, &slabs_free);
                block->poison = SLAB_POISON;
            }
        }
    }

    slab_block_t* block = list_front(&slabs_free, slab_block_t, list_entry);
    list_del(&block->list_entry);

    return block;
}

static slab_t* slab_create(slab_allocator_t* allocator)
{
    page_t* pages = page_alloc(
        allocator->slab_size / PAGE_SIZE,
        PAGE_ALLOC_CONT | PAGE_ALLOC_ZEROED);

    if (unlikely(!pages))
    {
        return NULL;
    }

    slab_t* slab = slab_entry_alloc();

    if (unlikely(!slab))
    {
        pages_free(pages);
        return NULL;
    }

    list_init(&slab->list_entry);
    list_init(&slab->free);
    list_add_tail(&slab->list_entry, &allocator->slabs);
    allocator->slabs_count++;
    slab->pages     = pages;
    slab->start     = page_virt(pages);
    slab->end       = page_virt(pages) + allocator->slab_size;
    slab->allocated = 0;

    void* ptr = page_virt_ptr(pages);

    for (size_t i = 0; i < allocator->slab_size / allocator->size; ++i, ptr += allocator->size)
    {
        slab_block_t* block = ptr(ptr);
        list_init(&block->list_entry);
        list_add_tail(&block->list_entry, &slab->free);
        block->poison = SLAB_POISON;
    }

    return slab;
}

static void* slab_block_get(slab_t* slab)
{
    slab_block_t* block = list_front(&slab->free, slab_block_t, list_entry);
    list_del(&block->list_entry);
    block->poison = 0;
    slab->allocated++;
    return block;
}

void* slab_alloc(size_t size)
{
    slab_t* slab;
    slab_allocator_t* allocator = slab_allocator_get(size);

    if (unlikely(!allocator))
    {
        return NULL;
    }

    scoped_mutex_lock(&allocator->lock);

    list_for_each_entry(slab, &allocator->slabs, list_entry)
    {
        if (unlikely(list_empty(&slab->free)))
        {
            continue;
        }

        return slab_block_get(slab);
    }

    slab = slab_create(allocator);

    if (unlikely(!slab))
    {
        return NULL;
    }

    return slab_block_get(slab);
}

void slab_free(void* ptr, size_t size)
{
    slab_t* slab;
    slab_block_t* block = ptr;
    slab_allocator_t* allocator = slab_allocator_get(size);

    if (unlikely(!allocator))
    {
        log_error("%s: ptr: %p, size: %zu: invalid size", __func__, ptr, size);
        return;
    }

    scoped_mutex_lock(&allocator->lock);

    list_for_each_entry(slab, &allocator->slabs, list_entry)
    {
        if (!(addr(ptr) >= slab->start && addr(ptr) < slab->end))
        {
            continue;
        }

        // FIXME: this can be randomly happen on some data which happens to set exact same value
        if (unlikely(block->poison == SLAB_POISON))
        {
            log_info("possibly freeing already free block %p", ptr);
        }

        list_init(&block->list_entry);

        list_add_tail(&block->list_entry, &slab->free);
        block->poison = SLAB_POISON;
        slab->allocated--;

#if SLAB_ZERO_AFTER_FREE
        memset(shift(block, sizeof(*block)), 0, allocator->size - sizeof(*block));
#endif

        return;
    }

    log_error("%s: ptr: %p, size: %zu: unknown pointer", __func__, ptr, size);
}
