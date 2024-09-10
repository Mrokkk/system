#pragma once

#include <stdbool.h>
#include <common/list.h>
#include <common/bitset.h>

#define CACHELINE_SIZE        64
#define METADATA_SIZE         0x10000
#define METADATA_INITIAL_SIZE 0x2000
#define SLAB_GUARD            0x32fb2002
#define LARGE_REGION_GUARD    0x3948fde
#define PAGE_SIZE             0x1000

#define DEFINE_CLASS(size, slab_size) \
    CLASS_##size = __COUNTER__, \
    CLASS_##size##_SIZE      = size, \
    CLASS_##size##_SLAB_SIZE = slab_size, \
    CLASS_##size##_SLOTS     = (slab_size / size)

#define CLASS(size, slab_size) \
    DEFINE_CLASS(size, slab_size),

#define CLASS_LAST(size) \
    CLASS_LAST_SIZE = size, \
    CLASSES_COUNT = CLASS_##size + 1, \

enum malloc_classes
{
#include "malloc_size_classes.h"
};

#undef CLASS
#undef CLASS_LAST

struct slab
{
    uintptr_t      guard;
    uintptr_t      start, end;
    list_head_t    list_entry;
    bitset_data_t  slots[BITSET_SIZE(256)];
    size_t         allocated;
    size_t         free;
    uintptr_t      guard2;
};

typedef struct slab slab_t;

struct slab_allocator
{
    uintptr_t      guard;
    size_t         class_size;
    size_t         mem_size;
    size_t         slots;
    list_head_t    slabs;
    uintptr_t      guard2;
};

typedef struct slab_allocator slab_allocator_t;

struct large_region
{
    uintptr_t   guard;
    uintptr_t   start, end;
    list_head_t list_entry;
    uintptr_t   guard2;
};

typedef struct large_region large_region_t;

struct metadata
{
    void*            end;
    void*            vaddr_end;
    void*            next_free;
    list_head_t      large_regions;
    struct
    {
        list_head_t  slabs;
        list_head_t  large_regions;
    } free;
    slab_allocator_t slab_allocators[CLASSES_COUNT];
};

typedef struct metadata metadata_t;
