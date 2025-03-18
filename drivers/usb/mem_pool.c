#include "mem_pool.h"

#include <kernel/mutex.h>
#include <kernel/bitset.h>
#include <kernel/malloc.h>

mem_pool_t* mem_pool_create(size_t block_size)
{
    if (unlikely((block_size % 8) || (block_size < MIN_BLOCK_SIZE)))
    {
        return NULL;
    }

    mem_pool_t* pool = zalloc(mem_pool_t);

    if (unlikely(!pool))
    {
        return NULL;
    }

    mutex_init(&pool->lock);
    bitset_initialize(pool->map);

    pool->block_size = block_size;
    pool->pages = page_alloc(POOL_SIZE / PAGE_SIZE, PAGE_ALLOC_CONT | PAGE_ALLOC_UNCACHED);

    if (unlikely(!pool->pages))
    {
        delete(pool);
        return NULL;
    }

    return pool;
}

mem_buffer_t* mem_pool_allocate(mem_pool_t* pool, size_t count)
{
    size_t size = count * pool->block_size;
    mem_buffer_t* buf = alloc(mem_buffer_t);

    if (unlikely(!buf))
    {
        return NULL;
    }

    scoped_mutex_lock(&pool->lock);

    int buffer_id = bitset_find_clear_range(pool->map, count);
    size_t offset = buffer_id * pool->block_size;

    if (unlikely(buffer_id < 0))
    {
        delete(buf);
        return NULL;
    }

    bitset_set_range(pool->map, buffer_id, count);

    memset(page_virt_ptr(pool->pages) + offset, 0, size);

    buf->paddr = page_phys(pool->pages) + offset;
    buf->vaddr = page_virt_ptr(pool->pages) + offset;
    buf->size = size;
    buf->pool = pool;

    return buf;
}

void mem_pool_free(mem_buffer_t* buf)
{
    mem_pool_t* pool = buf->pool;
    scoped_mutex_lock(&pool->lock);

    bitset_clear_range(
        pool->map,
        (buf->paddr - page_phys(pool->pages)) / pool->block_size,
        buf->size / pool->block_size);

    delete(buf);
}

void mem_pool_delete(mem_pool_t* pool)
{
    if (!pool)
    {
        return;
    }

    {
        scoped_mutex_lock(&pool->lock);
        pages_free(pool->pages);
    }

    delete(pool);
}
