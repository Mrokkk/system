#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bitset.h>

#define MALLOC_AREA         0x14000
#define MALLOC_BLOCK_SIZE   8

BITSET_DECLARE(bitset, MALLOC_AREA / MALLOC_BLOCK_SIZE);

static void* data;

#define ADDR(a) ((unsigned int)(a))
#define PTR(a) ((void*)(a))

#define ALIGN_TO(address, size) \
    (((address) + size - 1) & (~(size - 1)))

void* malloc(size_t size)
{
    size_t blocks = ALIGN_TO(size, MALLOC_BLOCK_SIZE) / MALLOC_BLOCK_SIZE;

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

void* calloc(size_t size)
{
    void* data = malloc(size);
    if (data)
    {
        memset(data, 0, size);
    }
    return data;
}

void free(void* PTR, size_t size)
{
    size_t frame = (ADDR(PTR) - ADDR(data)) / MALLOC_BLOCK_SIZE;
    size_t blocks = ALIGN_TO(size, MALLOC_BLOCK_SIZE) / MALLOC_BLOCK_SIZE;
    bitset_clear_range(bitset, frame, blocks);
}
