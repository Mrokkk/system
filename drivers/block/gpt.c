#include <kernel/gpt.h>

bool gpt_partition_valid(gpt_part_entry_t* p)
{
    for (size_t i = 0; i < sizeof(p->type_guid); ++i)
    {
        if (p->type_guid.b[i])
        {
            return true;
        }
    }
    return false;
}

size_t gpt_partition_count(void* start, void* end, uint32_t part_entry_size)
{
    gpt_part_entry_t* entry;
    size_t partition_count = 0;

    for (entry = start; addr(entry) < addr(end); entry = shift(entry, part_entry_size))
    {
        if (!gpt_partition_valid(entry))
        {
            continue;
        }

        partition_count++;
    }

    return partition_count;
}

gpt_part_entry_t* gpt_partition_next(gpt_part_entry_t* entry, gpt_part_entry_t* end, size_t part_entry_size)
{
    entry = shift(entry, part_entry_size);

    for (; addr(entry) < addr(end); entry = shift(entry, part_entry_size))
    {
        if (gpt_partition_valid(entry))
        {
            break;
        }
    }

    return entry;
}


