#include <stdlib.h>
#include <unistd.h>
#include <bitset.h>

#define MALLOC_AREA         0x4000
#define MALLOC_BLOCK_SIZE   8

BITSET_DECLARE(bitset, MALLOC_AREA / MALLOC_BLOCK_SIZE);

static void* data;

void bss_init()
{
    extern char _sbss[], _ebss[];
    __builtin_memset(_sbss, 0, _ebss - _sbss);
}

#define addr(a) ((unsigned int)(a))
#define ptr(a) ((void*)(a))

#define align_to_block_size(address, size) \
    (((address) + size - 1) & (~(size - 1)))

void* malloc(size_t size)
{
    size_t blocks = align_to_block_size(size, MALLOC_BLOCK_SIZE) / MALLOC_BLOCK_SIZE;

    if (!data)
    {
        data = sbrk(MALLOC_AREA);
    }

    int frame = bitset_find_clear_range(bitset, blocks);

    if (frame < 0)
    {
        return NULL;
    }

    bitset_set_range(bitset, frame, blocks);

    return data + frame * MALLOC_BLOCK_SIZE;
}

void free(void* ptr, size_t size)
{
    size_t frame = (addr(ptr) - addr(data)) / MALLOC_BLOCK_SIZE;
    size_t blocks = align_to_block_size(size, MALLOC_BLOCK_SIZE) / MALLOC_BLOCK_SIZE;
    bitset_clear_range(bitset, frame, blocks);
}
