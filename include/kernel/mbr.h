#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/compiler.h>

#define MBR_SECTOR_SIZE         512
#define MBR_SIGNATURE           0xaa55

struct mbr_partition
{
    uint8_t  attr;
    uint8_t  start_head;
    uint8_t  start_sect:6;
    uint16_t start_cyl:10;
    uint8_t  type;
    uint8_t  end_head;
    uint8_t  end_sector:6;
    uint16_t end_cyl:10;
    uint32_t lba_start;
    uint32_t sectors;
} PACKED;

#define MBR_PARTITION_LINUX 0x83
#define MBR_PARTITION_GPT   0xee

typedef struct mbr_partition mbr_partition_t;

struct mbr
{
    uint8_t         bootsect[440];
    uint32_t        id;
    uint16_t        reserved;
    mbr_partition_t partitions[4];
    uint16_t        signature;
} PACKED;

typedef struct mbr mbr_t;

static inline bool mbr_partition_valid(mbr_partition_t* p)
{
    return !!p->type;
}

bool mbr_is_gpt(mbr_t* mbr);
size_t mbr_partition_count(mbr_t* mbr);
mbr_partition_t* mbr_partition_next(mbr_partition_t* cur, mbr_partition_t* end);
const char* mbr_partition_name(mbr_partition_t* p);

#define MBR_FOR_EACH_PARTITION(mbr, p) \
    for (mbr_partition_t* p = (mbr)->partitions, *end = (mbr)->partitions + 4; \
        p < end; \
        p = mbr_partition_next(p, end))
