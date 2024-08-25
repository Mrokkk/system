#include <common/bitset.h>

int __bitset_find_clear_range(uint32_t* bitset, int count, size_t bitset_size)
{
    uint32_t data;
    uint32_t mask;
    int carry_over = count;
    int bits_to_set;
    int result_range = -1;
    int original_count = count;

    for (size_t i = 0; i < bitset_size; ++i)
    {
        data = ~bitset[i];
        if (!data)
        {
            continue;
        }
        for (size_t j = 0; j < BITSET_BITS; ++j, data = data >> 1)
        {
            carry_over = zero_clamp(carry_over - (BITSET_BITS - j));
            bits_to_set = count - carry_over;
            mask = (uint32_t)~0UL >> (BITSET_BITS - bits_to_set);
            if ((data & mask) == mask)
            {
                result_range = result_range < 0
                    ? (int)(i * BITSET_BITS + j)
                    : result_range;
                count -= bits_to_set;
                break;
            }
            else
            {
                carry_over = original_count;
                count = original_count;
                result_range = -1;
            }
        }
        if (carry_over == 0)
        {
            break;
        }
    }
    return result_range;
}
