#pragma once

#define KERNEL_PAGE_OFFSET  0xffff800000000000UL

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef uintptr_t pgprot_t;
typedef uintptr_t pte_t;
typedef uintptr_t pgd_t;
typedef uintptr_t pud_t;
typedef uintptr_t pmd_t;

#endif
