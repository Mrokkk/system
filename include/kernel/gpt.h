#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/compiler.h>

struct guid
{
    uint8_t b[16];
};

typedef struct guid guid_t;

struct gpt
{
    uint8_t  signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t checksum;
    uint32_t reserved;
    uint64_t lba_header;
    uint64_t lba_alt_header;
    uint64_t lba_start;
    uint64_t lba_end;
    guid_t   guid;
    uint64_t lba_part_entry;
    uint32_t part_entries;
    uint32_t part_entry_size;
    uint32_t part_entries_checksum;
};

typedef struct gpt gpt_t;

struct gpt_part_entry
{
    guid_t   type_guid;
    guid_t   unique_guid;
    uint64_t lba_start;
    uint64_t lba_end;
    uint64_t attr;
    uint16_t name[36];
};

typedef struct gpt_part_entry gpt_part_entry_t;

bool gpt_partition_valid(gpt_part_entry_t* p);
size_t gpt_partition_count(void* start, void* end, uint32_t part_entry_size);
gpt_part_entry_t* gpt_partition_next(gpt_part_entry_t* entry, gpt_part_entry_t* end, size_t part_entry_size);

#define GPT_FOR_EACH_PARTITION(entry, start, end, part_entry_size) \
    for (gpt_part_entry_t* entry = (start), *_end = (end); \
        addr(entry) < addr(_end); \
        entry = gpt_partition_next(entry, _end, part_entry_size))
