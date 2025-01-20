#pragma once

#include <kernel/compiler.h>
#include <kernel/page_types.h>

enum mmio_flags
{
    MMIO_UNCACHED       = 0,
    MMIO_WRITE_COMBINE  = 1,
    MMIO_WRITE_BACK     = 2,
    MMIO_WRITE_THROUGH  = 3,
};

#define mmio_map_uc(paddr, size, name) mmio_map(paddr, size, MMIO_UNCACHED, name)
#define mmio_map_wc(paddr, size, name) mmio_map(paddr, size, MMIO_WRITE_COMBINE, name)
#define mmio_map_wb(paddr, size, name) mmio_map(paddr, size, MMIO_WRITE_BACK, name)
#define mmio_map_wt(paddr, size, name) mmio_map(paddr, size, MMIO_WRITE_THROUGH, name)

MUST_CHECK(void*) mmio_map(uintptr_t paddr, uintptr_t size, int flags, const char* name);
void mmio_print(void);
