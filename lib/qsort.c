#include <stdlib.h>
#include <stdint.h>

#define ELEM(base, i, size) \
    ({ (void*)(ADDR(base) + (i) * (size)); })

typedef int (*comp_t)(const void*, const void*);
typedef int (*comp_d_t)(const void*, const void*, void*);

enum swap_type
{
    SWAP_DWORDS,
    SWAP_BYTES,
};

typedef enum swap_type swap_type_t;

static void swap(void* a, void* b, size_t n, swap_type_t swap_type)
{
    if (swap_type == SWAP_DWORDS)
    {
        do
        {
            n -= 4;
            int tmp = *(uint32_t*)(a + n);
            *(uint32_t*)(a + n) = *(uint32_t*)(b + n);
            *(uint32_t*)(b + n) = tmp;
        }
        while (n);
    }
    else
    {
        do
        {
            --n;
            int tmp = *(uint8_t*)(a + n);
            *(uint8_t*)(a + n) = *(uint8_t*)(b + n);
            *(uint8_t*)(b + n) = tmp;
        }
        while (n);
    }
}

static size_t partition(
    void* base,
    size_t size,
    size_t start,
    size_t end,
    swap_type_t swap_type,
    comp_d_t compar,
    void* arg)
{
    void* pivot = ELEM(base, end, size);

    size_t i = start;

    for (size_t j = start; j < end; ++j)
    {
        void* tmp = ELEM(base, j, size);
        void* tmp2 = ELEM(base, i, size);
        if (compar(tmp, pivot, arg) < 0)
        {
            swap(tmp, tmp2, size, swap_type);
            ++i;
        }
    }

    swap(ELEM(base, i, size), ELEM(base, end, size), size, swap_type);

    return i;
}

static void qsort_impl(
    void* base,
    size_t start,
    size_t end,
    size_t size,
    swap_type_t swap_type,
    comp_d_t compar,
    void* arg)
{
    if ((int)start >= (int)end || (int)start < 0)
    {
        return;
    }
    size_t p = partition(base, size, start, end, swap_type, compar, arg);
    qsort_impl(base, start, p - 1, size, swap_type, compar, arg);
    qsort_impl(base, p + 1, end,   size, swap_type, compar, arg);
}

static inline swap_type_t get_swap_type(void* base, size_t size)
{
    if ((size & (sizeof(uint32_t) - 1)) == 0 &&
        ADDR(base) % __alignof__(uint32_t) == 0)
    {
        return SWAP_DWORDS;
    }
    return SWAP_BYTES;
}

void LIBC(qsort)(
    void* base,
    size_t nmemb,
    size_t size,
    comp_t compar)
{
    return qsort_impl(base, 0, nmemb - 1, size, get_swap_type(base, size), (comp_d_t)((void*)compar), NULL);
}

void LIBC(qsort_r)(
    void* base,
    size_t nmemb,
    size_t size,
    comp_d_t compar,
    void* arg)
{
    return qsort_impl(base, 0, nmemb - 1, size, get_swap_type(base, size), compar, arg);
}

LIBC_ALIAS(qsort);
LIBC_ALIAS(qsort_r);
