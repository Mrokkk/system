#define log_fmt(fmt) "pat: " fmt
#include <arch/asm.h>
#include <arch/msr.h>
#include <arch/pat.h>
#include <arch/mtrr.h>
#include <arch/system.h>
#include <arch/register.h>

#include <kernel/cpu.h>
#include <kernel/errno.h>
#include <kernel/page_table.h>

static ALIGN(8) uint8_t pat_entries[8];
static pgprot_t type_to_pgprot[8];
bool pat;

pgprot_t pat_pgprot_get(cache_policy_t type)
{
    if (unlikely(type > CACHE_MAX || type == CACHE_RSVD1 || type == CACHE_RSVD2))
    {
        log_error("pat_pgprot_get: incorrect type: %u", type);
        return 0;
    }

    return type_to_pgprot[type];
}

int pat_initialize(void)
{
    if (!cpu_has(X86_FEATURE_PAT))
    {
        return -ENODEV;
    }

    // Remap PATs
    pat_entries[0] = CACHE_WB;       // 0
    pat_entries[1] = CACHE_WT;       // PWT
    pat_entries[2] = CACHE_UC;       // PCD
    pat_entries[3] = CACHE_UC_MINUS; // PCD | PWT
    pat_entries[4] = CACHE_WC;       // PAT
    pat_entries[5] = CACHE_WP;       // PAT | PWT
    pat_entries[6] = CACHE_UC;       // PAT | PCD
    pat_entries[7] = CACHE_UC_MINUS; // PAT | PCD | PWT

    type_to_pgprot[CACHE_UC]       = PAGE_PCD;
    type_to_pgprot[CACHE_WC]       = PAGE_PAT;
    type_to_pgprot[CACHE_WT]       = PAGE_PWT;
    type_to_pgprot[CACHE_WP]       = PAGE_PAT | PAGE_PWT;
    type_to_pgprot[CACHE_WB]       = 0;
    type_to_pgprot[CACHE_UC_MINUS] = PAGE_PCD | PAGE_PWT;

    log_info("supported;");

    for (int i = 0; i < 8; ++i)
    {
        log_continue(" PAT%u: %#x", i, pat_entries[i]);
    }

    wrmsrll(IA32_MSR_PAT, *(uint64_t*)pat_entries);

    pat = true;

    return 0;
}
