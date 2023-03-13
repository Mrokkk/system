#pragma once

#define MEMORY_BLOCK_SIZE       16
#define FAST_MALLOC_BLOCK_SIZE  8
#define FAST_MALLOC_AREA        (4096 * 2)

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>

#include <kernel/list.h>
#include <kernel/macro.h>
#include <kernel/trace.h>

void* fmalloc(size_t size);
int ffree(void* ptr, size_t size);
void fmalloc_init(void);
void fmalloc_stats_print(void);

void kmalloc_init(void);
void* kmalloc(unsigned int);
int kfree(void*);
void kmalloc_stats_print(void);

// Generic constructor
#define __CONSTRUCT(object, initializer) \
    ({ object = (typeof(object))kmalloc(sizeof(typeof(*object))); if (object) (void)initializer; !object; })

#define __CONSTRUCT2(type, initializer) \
    ({ type* this = (type*)kmalloc(sizeof(type)); if (this) { (void)initializer; } this; })

// Generic destructor
#define __DESTRUCT(object, deinitializer) \
    do { \
        (void)deinitializer; \
        kfree(object); \
    } while (0)

#define CONSTRUCT_1(object)              __CONSTRUCT(object, 0)
#define CONSTRUCT_2(object, initializer) __CONSTRUCT(object, initializer)

#define CONSTRUCT_3(type)              __CONSTRUCT2(type, 0)
#define CONSTRUCT_4(type, initializer) __CONSTRUCT2(type, initializer)

#define DESTRUCT_1(object)                __DESTRUCT(object, 0)
#define DESTRUCT_2(object, deinitializer) __DESTRUCT(object, deinitializer)

#define new(...) \
    REAL_VAR_MACRO_2(CONSTRUCT_1, CONSTRUCT_2, __VA_ARGS__)

#define delete(...) \
    REAL_VAR_MACRO_2(DESTRUCT_1, DESTRUCT_2, __VA_ARGS__)

#define alloc(...) \
    REAL_VAR_MACRO_2(CONSTRUCT_3, CONSTRUCT_4, __VA_ARGS__)

#endif // __ASSEMBLER__
