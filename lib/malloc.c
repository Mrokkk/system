#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <common/bitset.h>

#include "malloc.h"

static metadata_t* metadata;

[[noreturn]] static void die(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

[[noreturn]] static void die(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    [[noreturn]] extern void LIBC(abort)(void);
    LIBC(abort)();
}

#define DIE(fmt, ...) \
    die("%s(): " fmt "\n", __func__, ##__VA_ARGS__)

#define DIE_PERROR(fmt, ...) \
    die("%s(): " fmt ": %s\n", __func__, ##__VA_ARGS__, strerror(errno))

static void class_metadata_set(slab_allocator_t* allocator, size_t class_size, size_t mem_size, size_t slots)
{
    allocator->guard      = SLAB_GUARD;
    allocator->guard2     = SLAB_GUARD;
    allocator->class_size = class_size;
    allocator->mem_size   = mem_size;
    allocator->slots      = slots;
    list_init(&allocator->slabs);
}

static void metadata_init(void)
{
    metadata = mmap(NULL, METADATA_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (UNLIKELY(!metadata))
    {
        DIE_PERROR("cannot allocate memory for metadata");
    }

    void* metadata_end = SHIFT(metadata, METADATA_SIZE - PAGE_SIZE);
    metadata = SHIFT(metadata, PAGE_SIZE * (rand() % 3 + 1));

    if (UNLIKELY(mprotect(metadata, METADATA_INITIAL_SIZE, PROT_READ | PROT_WRITE)))
    {
        DIE_PERROR("cannot mprotect metadata");
    }

    list_init(&metadata->large_regions);
    list_init(&metadata->free.large_regions);
    list_init(&metadata->free.slabs);
    metadata->end = SHIFT(metadata, METADATA_INITIAL_SIZE);
    metadata->vaddr_end = metadata_end;
    metadata->next_free = PTR(ALIGN_TO(ADDR(metadata + 1), CACHELINE_SIZE));

#define CLASS(size, slab_size) \
    class_metadata_set( \
        &metadata->slab_allocators[CLASS_##size], \
        CLASS_##size##_SIZE, \
        CLASS_##size##_SLAB_SIZE, \
        CLASS_##size##_SLOTS);

#include "malloc_size_classes.h"
}

static slab_allocator_t* slab_allocator_find(size_t size)
{
    slab_allocator_t* allocator = NULL;

    if (size <= 128)
    {
        allocator = metadata->slab_allocators + ((size - 1) >> 4);
    }
    else if (size > 128 && size <= 256)
    {
        allocator = metadata->slab_allocators + ((size - 128 - 1) >> 5) + CLASS_128 + 1;
    }
    else if (size > 256 && size <= 512)
    {
        allocator = metadata->slab_allocators + ((size - 256 - 1) >> 6) + CLASS_256 + 1;
    }
    else if (size > 512 && size <= 1024)
    {
        allocator = metadata->slab_allocators + ((size - 512 - 1) >> 7) + CLASS_512 + 1;
    }
    else if (size > 1024 && size <= 2048)
    {
        allocator = metadata->slab_allocators + ((size - 1024 - 1) >> 8) + CLASS_1024 + 1;
    }
    else if (size > 2048 && size <= 4096)
    {
        allocator = metadata->slab_allocators + ((size - 2048 - 1) >> 9) + CLASS_2048 + 1;
    }
    else if (size > 4096 && size <= 8192)
    {
        allocator = metadata->slab_allocators + ((size - 4096 - 1) >> 10) + CLASS_4096 + 1;
    }
    else if (size > 8192 && size <= 16384)
    {
        allocator = metadata->slab_allocators + ((size - 8192 - 1) >> 11) + CLASS_8192 + 1;
    }
    else
    {
        return NULL;
    }

    if (UNLIKELY(allocator->guard != SLAB_GUARD || allocator->guard2 != SLAB_GUARD))
    {
        DIE("memory corruption: guards: %#x, %#x", allocator->guard, allocator->guard2);
    }

    return allocator;
}

static void* metadata_alloc(size_t size)
{
    void* ptr = metadata->next_free;

    if (UNLIKELY(ADDR(ptr) >= ADDR(metadata->end)))
    {
        if (UNLIKELY(ADDR(ptr) >= ADDR(metadata->vaddr_end)))
        {
            void* new_metadata = mmap(NULL, METADATA_SIZE, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

            if (UNLIKELY(!new_metadata))
            {
                return NULL;
            }

            ptr = new_metadata + PAGE_SIZE;

            if (UNLIKELY(mprotect(ptr, METADATA_INITIAL_SIZE, PROT_READ | PROT_WRITE)))
            {
                DIE_PERROR("cannot mprotect metadata");
            }

            metadata->next_free = ptr + ALIGN_TO(size, CACHELINE_SIZE);
            metadata->vaddr_end = new_metadata + METADATA_SIZE;
            metadata->end       = ptr + METADATA_INITIAL_SIZE;

            return ptr;
        }

        uintptr_t new_end = MIN(ADDR(metadata->vaddr_end), ADDR(metadata->end) + 2 * PAGE_SIZE);

        if (UNLIKELY(mprotect(metadata->end, new_end - ADDR(metadata->end), PROT_WRITE | PROT_READ)))
        {
            DIE_PERROR("cannot mprotect metadata");
        }

        metadata->end = PTR(new_end);
    }

    metadata->next_free += ALIGN_TO(size, CACHELINE_SIZE);
    return ptr;
}

static size_t size_align_for_canary(size_t size)
{
    if (size <= CLASS_LAST_SIZE)
    {
        return size + sizeof(uintptr_t);
    }
    return size;
}

static void slab_canary_set(void* ptr, slab_allocator_t* allocator)
{
    *SHIFT_AS(uintptr_t*, ptr, allocator->class_size - sizeof(uintptr_t)) = CANARY;
}

static bool slab_canary_check(void* ptr, slab_allocator_t* allocator)
{
    return *SHIFT_AS(uintptr_t*, ptr, allocator->class_size - sizeof(uintptr_t)) != CANARY;
}

static void* allocate_small(size_t size)
{
    int slot;
    slab_t* slab;
    void* mem;
    void* brk_ptr;
    slab_allocator_t* allocator;

    if (UNLIKELY(!metadata))
    {
        metadata_init();
    }

    if (UNLIKELY(!(allocator = slab_allocator_find(size))))
    {
        return NULL;
    }

    list_for_each_entry(slab, &allocator->slabs, list_entry)
    {
        if (slab->allocated < allocator->slots)
        {
            goto found_region;
        }
    }

    if (UNLIKELY(!(brk_ptr = sbrk(allocator->mem_size))))
    {
        return NULL;
    }

    if (UNLIKELY(!(slab = metadata_alloc(sizeof(*slab)))))
    {
        return NULL;
    }

    memset(slab, 0, sizeof(*slab));

    slab->start = ADDR(brk_ptr);
    slab->end = slab->start + allocator->mem_size;
    slab->free = allocator->mem_size;

    list_add(&slab->list_entry, &allocator->slabs);

found_region:
    slot = bitset_find_zero(slab->slots, allocator->slots);

    if (UNLIKELY(slot == -1))
    {
        return NULL;
    }

    slab->allocated++;
    bitset_set(slab->slots, slot);

    mem = PTR(slab->start + slot * allocator->class_size);

    if (CONFIG_WRITE_ZERO_AFTER_FREE)
    {
        uintptr_t* ptr = mem;
        uintptr_t end = ADDR(mem) + allocator->class_size;
        for (; ADDR(ptr) < end; ++ptr)
        {
            if (UNLIKELY(*ptr != 0))
            {
                DIE("overflow at %p", ptr);
            }
        }
    }

    slab_canary_set(mem, allocator);

    return mem;
}

static large_region_t* large_region_get(void)
{
    large_region_t* region;

    if (!list_empty(&metadata->free.large_regions))
    {
        region = list_front(&metadata->free.large_regions, large_region_t, list_entry);
        list_del(&region->list_entry);
        return region;
    }

    return metadata_alloc(sizeof(*region));
}

static void* allocate_large(size_t size)
{
    size = ALIGN_TO(size, PAGE_SIZE);

    void* range = mmap(NULL, size + 2 * PAGE_SIZE, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (UNLIKELY(!range))
    {
        return NULL;
    }

    if (UNLIKELY(mprotect(range + PAGE_SIZE, size, PROT_READ | PROT_WRITE)))
    {
        DIE_PERROR("failed to mprotect large region");
    }

    large_region_t* region = large_region_get();

    if (UNLIKELY(!region))
    {
        return NULL;
    }

    list_init(&region->list_entry);
    region->start = ADDR(range + PAGE_SIZE);
    region->end = region->start + size;
    region->guard = LARGE_REGION_GUARD;
    region->guard2 = LARGE_REGION_GUARD;

    list_add(&region->list_entry, &metadata->large_regions);

    return range + PAGE_SIZE;
}

void* LIBC(malloc)(size_t size)
{
    VALIDATE_INPUT(size, NULL);
    size = size_align_for_canary(size);
    return size <= CLASS_LAST_SIZE ? allocate_small(size) : allocate_large(size);
}

void* LIBC(calloc)(size_t nmemb, size_t size)
{
    void* ptr = LIBC(malloc)(size * nmemb);

    if (UNLIKELY(!ptr))
    {
        return NULL;
    }

    if (!CONFIG_WRITE_ZERO_AFTER_FREE)
    {
        memset(ptr, 0, size * nmemb);
    }
    return ptr;
}

static int slab_allocator_and_region_find(void* ptr, slab_allocator_t** allocator, slab_t** slab)
{
    slab_allocator_t* temp_allocator;
    slab_t* temp_slab;

    for (int i = 0; i < CLASSES_COUNT; ++i)
    {
        temp_allocator = &metadata->slab_allocators[i];

        list_for_each_entry(temp_slab, &temp_allocator->slabs, list_entry)
        {
            if (ADDR(ptr) >= temp_slab->start && ADDR(ptr) < temp_slab->end)
            {
                *allocator = temp_allocator;
                *slab      = temp_slab;
                return 0;
            }
        }
    }

    return -1;
}

static int large_region_find(void* ptr, large_region_t** region)
{
    large_region_t* temp_region;

    list_for_each_entry(temp_region, &metadata->large_regions, list_entry)
    {
        if (ADDR(ptr) >= temp_region->start && ADDR(ptr) < temp_region->end)
        {
            *region = temp_region;
            return 0;
        }
    }

    return -1;
}

static void small_free(void* ptr, slab_allocator_t* allocator, slab_t* slab)
{
    off_t offset = ADDR(ptr) - slab->start;

    if (UNLIKELY(offset % allocator->class_size))
    {
        DIE("%p not aligned for class size %lu", ptr, allocator->class_size);
    }

    int slot = offset / allocator->class_size;

    if (UNLIKELY(!bitset_test(slab->slots, slot)))
    {
        DIE("double free of %p detected", ptr);
    }

    if (UNLIKELY(slab_canary_check(ptr, allocator)))
    {
        DIE("overflow detected in %p", ptr);
    }

    bitset_clear(slab->slots, slot);
    slab->allocated--;

    if (CONFIG_WRITE_ZERO_AFTER_FREE)
    {
        memset(ptr, 0, allocator->class_size);
    }
}

static void large_free(void*, large_region_t* region)
{
    if (UNLIKELY(munmap(PTR(region->start - PAGE_SIZE), region->end - region->start + 2 * PAGE_SIZE)))
    {
        DIE("munmap failed: %s", strerror(errno));
    }

    list_del(&region->list_entry);
    list_add(&region->list_entry, &metadata->free.large_regions);
}

void LIBC(free)(void* ptr)
{
    slab_t* slab;
    slab_allocator_t* allocator;
    large_region_t* large_region;

    if (UNLIKELY(!ptr))
    {
        return;
    }

    if (UNLIKELY(!metadata))
    {
        DIE("metadata is null");
    }

    if (LIKELY(!slab_allocator_and_region_find(ptr, &allocator, &slab)))
    {
        return small_free(ptr, allocator, slab);
    }

    if (LIKELY(!large_region_find(ptr, &large_region)))
    {
        return large_free(ptr, large_region);
    }

    DIE("unknown ptr %p", ptr);
}

static void* small_realloc(void* ptr, size_t size, slab_allocator_t* allocator, slab_t* slab)
{
    void* new_ptr;
    size_t to_copy = MIN(size, allocator->class_size);
    uintptr_t offset = ADDR(ptr) - slab->start;
    size_t class_size = allocator->class_size;
    int diff = class_size - size;

    if (UNLIKELY(offset % class_size))
    {
        DIE("not aligned ptr %p for class size %lu", ptr, class_size);
    }

    if (diff >= 0 && diff < (int)(class_size / 2))
    {
        return ptr;
    }

    if (UNLIKELY(!(new_ptr = LIBC(malloc)(size))))
    {
        return NULL;
    }

    memcpy(new_ptr, ptr, to_copy);

    LIBC(free)(ptr);

    return new_ptr;
}

static void* large_realloc(void* ptr, size_t size, large_region_t* region)
{
    void* new_ptr;
    size_t to_copy = MIN(size, region->end - region->start);
    size_t region_size = region->end - region->start;
    int diff = region_size - size;

    if (diff >= 0 && diff < (int)(region_size / 2))
    {
        return ptr;
    }

    if (UNLIKELY(!(new_ptr = LIBC(malloc)(size))))
    {
        return NULL;
    }

    memcpy(new_ptr, ptr, to_copy);

    LIBC(free)(ptr);

    return new_ptr;
}

void* LIBC(realloc)(void* ptr, size_t size)
{
    slab_t* slab;
    slab_allocator_t* allocator;
    large_region_t* large_region;

    if (UNLIKELY(!ptr))
    {
        return LIBC(malloc(size));
    }

    if (UNLIKELY(!metadata))
    {
        DIE("metadata is null");
    }

    if (LIKELY(!slab_allocator_and_region_find(ptr, &allocator, &slab)))
    {
        return small_realloc(ptr, size, allocator, slab);
    }

    if (LIKELY(!large_region_find(ptr, &large_region)))
    {
        return large_realloc(ptr, size, large_region);
    }

    DIE("unknown ptr %p", ptr);
}

LIBC_ALIAS(malloc);
LIBC_ALIAS(calloc);
LIBC_ALIAS(realloc);
LIBC_ALIAS(free);
