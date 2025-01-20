#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <arch/msr.h>
#include <arch/cache.h>

void mtrr_initialize(void);
int mtrr_add(uintptr_t paddr, size_t size, cache_policy_t type);

static inline bool mtrr_enabled(void)
{
    extern bool mtrr;
    return mtrr;
}
