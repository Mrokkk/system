#include <kernel/page.h>
#include <kernel/bitset.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>

static char* fmalloc_mem;
static BITSET_DECLARE(bitset, FAST_MALLOC_AREA / FAST_MALLOC_BLOCK_SIZE);

static struct fmalloc_stats
{
    size_t fmalloc_calls;
    size_t ffree_calls;
} fmalloc_stats;

void* fmalloc(size_t size)
{
    size_t blocks = align(size, FAST_MALLOC_BLOCK_SIZE) / FAST_MALLOC_BLOCK_SIZE;
    ++fmalloc_stats.fmalloc_calls;

    int frame = bitset_find_clear_range(bitset, blocks);

    if (frame < 0)
    {
        log_debug(DEBUG_FMALLOC, "cannot allocate");
        return NULL;
    }

    log_debug(DEBUG_FMALLOC, "allocating %u blocks at pos:%u", blocks, frame);

    bitset_set_range(bitset, frame, blocks);

    return fmalloc_mem + frame * FAST_MALLOC_BLOCK_SIZE;
}

int ffree(void* ptr, size_t size)
{
    size_t frame = (addr(ptr) - addr(fmalloc_mem)) / FAST_MALLOC_BLOCK_SIZE;
    size_t blocks = align(size, FAST_MALLOC_BLOCK_SIZE) / FAST_MALLOC_BLOCK_SIZE;

    log_debug(DEBUG_FMALLOC, "freeing %x, %u blocks at pos:%u", ptr, blocks, frame);

    bitset_clear_range(bitset, frame, blocks);

    ++fmalloc_stats.ffree_calls;
    return 0;
}

UNMAP_AFTER_INIT void fmalloc_init()
{
    size_t required_pages = FAST_MALLOC_AREA / PAGE_SIZE;
    page_t* page_range = page_alloc(required_pages, PAGE_ALLOC_CONT);

    if (unlikely(!page_range))
    {
        panic("cannot allocate %u pages for fmalloc!", required_pages);
    }

    fmalloc_mem = page_virt_ptr(page_range);
}

void fmalloc_stats_print()
{
    size_t used_blocks = bitset_count_set(bitset);

    log_info("used=%u B", used_blocks * FAST_MALLOC_BLOCK_SIZE);
    log_info("free=%u B", FAST_MALLOC_AREA - used_blocks * FAST_MALLOC_BLOCK_SIZE);
    log_info("fmalloc_cals=%u", fmalloc_stats.fmalloc_calls);
    log_info("ffree_cals=%u", fmalloc_stats.ffree_calls);
}
