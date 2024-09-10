#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint32_t bitset_data_t;

#define BITSET_BITS 32

#define BITSET_SIZE(count) ((count) / BITSET_BITS)

#define BITSET_DECLARE(name, count) \
    bitset_data_t name[BITSET_SIZE(count)]

#define bitset_initialize(name) \
    __builtin_memset(name, 0, sizeof(name))

static inline void bitset_initialize_set(uint32_t* bitset, size_t set_bits)
{
    uint32_t array_pos = set_bits / BITSET_BITS;
    uint32_t bit_pos = set_bits % BITSET_BITS;
    for (uint32_t i = 0; i < array_pos; bitset[i++] = ~0UL);
    bitset[array_pos] = (~0UL >> (BITSET_BITS - bit_pos));
}

static inline int bitset_test(uint32_t* bitset, size_t pos)
{
    uint32_t array_pos = pos / BITSET_BITS;
    uint32_t bit_pos = pos % BITSET_BITS;
    return bitset[array_pos] & (1 << bit_pos);
}

static inline int bitset_set(uint32_t* bitset, size_t pos)
{
    uint32_t array_pos = pos / BITSET_BITS;
    uint32_t bit_pos = pos % BITSET_BITS;
    return bitset[array_pos] |= 1 << bit_pos;
}

static inline int bitset_clear(uint32_t* bitset, size_t pos)
{
    uint32_t array_pos = pos / BITSET_BITS;
    uint32_t bit_pos = pos % BITSET_BITS;
    return bitset[array_pos] &= ~(1 << bit_pos);
}

static inline uint32_t zero_clamp(int value)
{
    return value < 0 ? 0 : value;
}

static inline void bitset_set_range(uint32_t* bitset, size_t pos, int count)
{
    uint32_t array_pos = pos / BITSET_BITS;
    uint32_t bit_pos = pos % BITSET_BITS;
    uint32_t mask, bits_to_set;

    for (int carry_over = count; carry_over; ++array_pos, bit_pos = 0, count -= bits_to_set)
    {
        carry_over = zero_clamp(carry_over - (BITSET_BITS - bit_pos));
        bits_to_set = count - carry_over;
        mask = (uint32_t)~0UL >> (BITSET_BITS - bits_to_set);
        bitset[array_pos] |= mask << bit_pos;
    }
}

static inline void bitset_clear_range(uint32_t* bitset, size_t pos, int count)
{
    uint32_t array_pos = pos / BITSET_BITS;
    uint32_t bit_pos = pos % BITSET_BITS;
    uint32_t mask, bits_to_clear;

    for (int carry_over = count; carry_over; ++array_pos, bit_pos = 0, count -= bits_to_clear)
    {
        carry_over = zero_clamp(carry_over - (BITSET_BITS - bit_pos));
        bits_to_clear = count - carry_over;
        mask = (uint32_t)~0UL >> (BITSET_BITS - bits_to_clear);
        bitset[array_pos] &= ~(mask << bit_pos);
    }
}

static inline int bitset_find_zero(uint32_t* bitset, size_t size)
{
    size_t index = 0;
    for (size_t i = 0; i < size;
        ({ ++i; if ((i % BITSET_BITS) == 0) index++; (void)i; }))
    {
        if (!(bitset[index] & (1 << i)))
        {
            return i;
        }
    }
    return -1;
}

int __bitset_find_clear_range(uint32_t* bitset, int count, size_t bitset_size);

#define bitset_find_clear_range(bitset, count) \
    __bitset_find_clear_range(bitset, count, sizeof(bitset) / 4)

static inline size_t __count_bits(uint32_t value)
{
    size_t count = 0;
    while (value)
    {
        value &= value - 1;
        count++;
    }
    return count;
}

#define bitset_count_set(bitset) \
    ({ \
        size_t count = 0; \
        for (size_t i = 0; i < sizeof(bitset) / 4; ++i) \
        { \
            count += __count_bits(bitset[i]); \
        } \
        count; \
    })
