#ifndef __MALLOC_H_
#define __MALLOC_H_

#define MEMORY_BLOCK_SIZE 16

#ifndef __ASSEMBLER__

#include <kernel/list.h>
#include <kernel/macro.h>

struct memory_block {
    union {
        struct {
            unsigned int size;
            char free;
            struct list_head blocks;
        };
        struct {
            unsigned char dummy[MEMORY_BLOCK_SIZE];
            unsigned int block_ptr[0];
        };
    };
};

void *kmalloc(unsigned int);
int kfree(void *);

/* Generic constructor */
#define __CONSTRUCT(object, initializer) \
    ({ object = (typeof(object))kmalloc(sizeof(typeof(*object))); \
    if (object) (void)initializer; !object; })

/* Generic destructor */
#define __DESTRUCT(object, deinitializer) \
    do { \
        (void)deinitializer; \
        kfree(object); \
    } while (0)

#define CONSTRUCT_1(object)              __CONSTRUCT(object, 0)
#define CONSTRUCT_2(object, initializer) __CONSTRUCT(object, initializer)

#define DESTRUCT_1(object)                __DESTRUCT(object, 0)
#define DESTRUCT_2(object, deinitializer) __DESTRUCT(object, deinitializer)

#define new(...) \
    REAL_VAR_MACRO_2(CONSTRUCT_1, CONSTRUCT_2, __VA_ARGS__)

#define delete(...) \
    REAL_VAR_MACRO_2(DESTRUCT_1, DESTRUCT_2, __VA_ARGS__)

#endif

#endif
