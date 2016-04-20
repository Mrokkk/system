#ifndef __MALLOC_H_
#define __MALLOC_H_

#define MEMORY_BLOCK_SIZE 16

#ifndef __ASSEMBLER__

struct memory_block {
    union {
        struct {
            unsigned int size;
            char free;
            struct memory_block *next;
        };
        struct {
            unsigned char dummy[MEMORY_BLOCK_SIZE];
            unsigned int block_ptr[0];
        };
    };
};

void *kmalloc(unsigned int);
int kfree(void *);

#endif

#endif
