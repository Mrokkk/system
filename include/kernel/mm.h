#ifndef __MM_H_
#define __MM_H_

#include <kernel/kernel.h>

struct mmap {
    char type;
    unsigned int base;
    unsigned int size;
};

#define MMAP_TYPE_AVL   1
#define MMAP_TYPE_DEV   2
#define MMAP_TYPE_NA    3
#define MMAP_TYPE_NDEF  4

extern struct mmap mmap[];

void mmap_print();

#endif /* __MM_H_ */
