#pragma once

#include <stdint.h>
#include <kernel/compiler.h>

#define MBR_SECTOR_SIZE         512
#define MBR_SIGNATURE           0xaa55

struct partition
{
    uint8_t attr;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t sectors;
} PACKED;

typedef struct partition partition_t;

struct mbr
{
    uint8_t boot[440];
    uint32_t id;
    uint16_t reserved;
    partition_t partitions[4];
    uint16_t signature;
} PACKED;

typedef struct mbr mbr_t;
