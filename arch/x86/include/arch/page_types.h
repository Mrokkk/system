#pragma once

#define PAGE_SIZE           0x1000
#define PAGE_MASK           0xfffUL
#define PAGE_ADDRESS        (~PAGE_MASK)
#define INIT_PGT_SIZE       4
#define PAGE_KERNEL_FLAGS   0x3

#ifdef __x86_64__
#include <arch/page_types_64.h>
#else
#include <arch/page_types_32.h>
#endif
