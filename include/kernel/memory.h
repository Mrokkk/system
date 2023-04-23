#pragma once

#include <stdint.h>
#include <kernel/kernel.h>

#define MMAP_TYPE_AVL   1
#define MMAP_TYPE_DEV   2
#define MMAP_TYPE_RES   3
#define MMAP_TYPE_NDEF  4

#define MEMORY_AREAS_SIZE 10

typedef struct memory_area
{
    uint32_t base;
    uint32_t size;
    int type;
} memory_area_t;

void memory_area_add(uint32_t base, uint32_t size, int type);

static inline const char* memory_area_type_string(int type)
{
    switch (type)
    {
        case MMAP_TYPE_AVL: return "available";
        case MMAP_TYPE_DEV: return "device";
        case MMAP_TYPE_RES: return "reserved";
        default: return "undefined";
    }
}

#define memory_areas_print() \
    { \
        memory_area_t* ma; \
        for (unsigned i = 0; i < MEMORY_AREAS_SIZE; ++i) \
        { \
            ma = &memory_areas[i]; \
            if (ma->size == 0) continue; \
            log_info("[mem %08x - %08x] %s", \
                ma->base, \
                ma->base + ma->size - 1, \
                memory_area_type_string(ma->type)); \
        } \
    }

extern memory_area_t memory_areas[];
extern uint32_t ram;
