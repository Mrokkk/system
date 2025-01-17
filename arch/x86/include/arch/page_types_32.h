#pragma once

#define KERNEL_PAGE_OFFSET  0xc0000000UL
#define KERNEL_MMIO_START   0x0fd000000ULL
#define KERNEL_MMIO_END     0x100000000ULL

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef uintptr_t pgprot_t;
typedef uintptr_t pte_t;
typedef uintptr_t pgd_t;
typedef uintptr_t pud_t;
typedef uintptr_t pmd_t;

#endif
