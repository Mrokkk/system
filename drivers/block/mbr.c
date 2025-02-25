#include <kernel/mbr.h>

bool mbr_is_gpt(mbr_t* mbr)
{
    size_t partitions = 0;

    for (int part_id = 0; part_id < 4; ++part_id)
    {
        mbr_partition_t* p = mbr->partitions + part_id;

        if (mbr_partition_valid(p))
        {
            partitions++;
        }
    }

    return partitions == 1 && mbr->partitions[0].type == MBR_PARTITION_GPT;
}

size_t mbr_partition_count(mbr_t* mbr)
{
    size_t partition_count = 0;

    for (int part_id = 0; part_id < 4; ++part_id)
    {
        mbr_partition_t* p = mbr->partitions + part_id;

        if (!mbr_partition_valid(p))
        {
            continue;
        }

        partition_count++;
    }

    return partition_count;
}

mbr_partition_t* mbr_partition_next(mbr_partition_t* cur, mbr_partition_t* end)
{
    for (++cur; cur != end; ++cur)
    {
        if (mbr_partition_valid(cur))
        {
            break;
        }
    }
    return cur;
}

const char* mbr_partition_name(mbr_partition_t* p)
{
    switch (p->type)
    {
        case MBR_PARTITION_LINUX: return "Linux partition";
        case MBR_PARTITION_GPT:   return "GPT partition";
        default:                  return "Unknown partition";
    }
}
