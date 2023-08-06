#pragma once

#define MEMORY_BLOCK_SIZE       16
#define FAST_MALLOC_BLOCK_SIZE  8
#define FAST_MALLOC_AREA        (4096 * 2)

#include <stddef.h>
#include <stdint.h>

#include <kernel/list.h>
#include <kernel/macro.h>
#include <kernel/trace.h>

MUST_CHECK(void*) fmalloc(size_t size);
int ffree(void* ptr, size_t size);
void fmalloc_init(void);
void fmalloc_stats_print(void);

MUST_CHECK(void*) slab_alloc(size_t size);
void slab_free(void* ptr, size_t size);
void slab_allocator_init(void);

#define USE_SLAB_ALLOCATION 1

#define __CONSTRUCT(type, init) \
    ({ type* this = slab_alloc(sizeof(type)); if (likely(this)) { (void)init; } this; })

#define __DESTRUCT(object, deinit) \
    ({ (void)deinit; slab_free(object, sizeof(*object)); })

#define alloc_array(type, count) \
    ({ type* this = slab_alloc(sizeof(type) * count); this; })

#define delete_array(object, count) \
    ({ slab_free(object, sizeof(*object) * count); })

#define CONSTRUCT_1(type)           __CONSTRUCT(type, 0)
#define CONSTRUCT_2(type, init)     __CONSTRUCT(type, init)

#define DESTRUCT_1(object)          __DESTRUCT(object, 0)
#define DESTRUCT_2(object, deinit)  __DESTRUCT(object, deinit)

#define alloc(...)      REAL_VAR_MACRO_2(CONSTRUCT_1, CONSTRUCT_2, __VA_ARGS__)
#define delete(...)     REAL_VAR_MACRO_2(DESTRUCT_1, DESTRUCT_2, __VA_ARGS__)
