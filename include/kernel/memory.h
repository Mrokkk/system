#pragma once

#include <stdint.h>
#include <kernel/kernel.h>

#define MMAP_TYPE_LOW   0
#define MMAP_TYPE_AVL   1
#define MMAP_TYPE_DEV   2
#define MMAP_TYPE_RES   3
#define MMAP_TYPE_NDEF  4

#define MEMORY_AREAS_SIZE 32

struct memory_area
{
    uint64_t start;
    uint64_t end;
    int type;
};

typedef struct memory_area memory_area_t;

void memory_area_add(uint64_t start, uint64_t end, int type);

static inline const char* memory_area_type_string(int type)
{
    switch (type)
    {
        case MMAP_TYPE_LOW: return "low";
        case MMAP_TYPE_AVL: return "available";
        case MMAP_TYPE_DEV: return "device";
        case MMAP_TYPE_RES: return "reserved";
        default: return "undefined";
    }
}

#define memory_areas_print() \
    { \
        memory_area_t* ma; \
        for (int i = 0; i < MEMORY_AREAS_SIZE; ++i) \
        { \
            ma = &memory_areas[i]; \
            if (ma->end == 0) continue; \
            log_notice("[mem %08llx - %08llx] %s", \
                ma->start, \
                ma->end - 1, \
                memory_area_type_string(ma->type)); \
        } \
    }

extern memory_area_t memory_areas[];
extern uint32_t usable_ram;
extern uint64_t full_ram;
extern uint32_t last_phys_address;
