#pragma once

#include <stdbool.h>
#include <arch/cache.h>
#include <kernel/page_types.h>

int pat_initialize(void);
pgprot_t pat_pgprot_get(cache_policy_t type);

static inline bool pat_enabled(void)
{
    extern bool pat;
    return pat;
}
