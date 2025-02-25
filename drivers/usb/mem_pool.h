#pragma once

#include <kernel/mutex.h>
#include <kernel/bitset.h>
#include <kernel/semaphore.h>
#include <kernel/page_alloc.h>

#define POOL_SIZE             (2 * PAGE_SIZE)
#define MIN_BLOCK_SIZE        16
#define MAX_POOL_BLOCKS_COUNT (POOL_SIZE / MIN_BLOCK_SIZE)

typedef struct mem_pool mem_pool_t;
typedef struct mem_buffer mem_buffer_t;

struct mem_pool
{
    size_t        block_size;
    page_t*       pages;
    mutex_t       lock;
    BITSET_DECLARE(map, MAX_POOL_BLOCKS_COUNT);
};

struct mem_buffer
{
    void*        vaddr;
    uintptr_t    paddr;
    size_t       size;
    mem_pool_t* pool;
};

mem_pool_t* mem_pool_create(size_t block_size);
mem_buffer_t* mem_pool_allocate(mem_pool_t* pool, size_t count);
void mem_pool_free(mem_buffer_t* buf);
void mem_pool_delete(mem_pool_t* pool);
