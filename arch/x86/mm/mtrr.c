#define log_fmt(fmt) "mtrr: " fmt
#include <stdint.h>

#include <arch/asm.h>
#include <arch/msr.h>
#include <arch/pat.h>
#include <arch/mtrr.h>
#include <arch/cache.h>
#include <arch/system.h>
#include <arch/register.h>

#include <kernel/cpu.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>

#define DEBUG_MTRR     0
#define MTRR_FIX_COUNT (8 * 11)

struct mtrr_context
{
    flags_t   flags;
    uintptr_t cr4;
    uint64_t  deftype;
};

struct mtrr_var_entry
{
    uint8_t        valid:1;
    cache_policy_t type:3;
    uintptr_t      paddr;
    size_t         size;
};

typedef struct mtrr_context mtrr_context_t;
typedef struct mtrr_var_entry mtrr_var_entry_t;

bool mtrr;
static bool mtrr_wc;
static size_t mtrr_var_size;
static mtrr_var_entry_t* mtrr_var_entries;
static MUTEX_DECLARE(lock);

static uint64_t mtrr_paddr_get(uint64_t base)
{
    return base & ~(0xfffULL);
}

static uint64_t mtrr_size_get(uint64_t mask)
{
    return (~(mask & ~0xfff) + 1);
}

static void mtrr_change_enter(mtrr_context_t* ctx)
{
    uintptr_t tmp;
    uint64_t deftype;

    irq_save(ctx->flags);

    if (cpu_has(X86_FEATURE_PGE))
    {
        asm volatile(
            "mov %%cr4, %0;"
            "mov %0, %1;"
            "andb $0x7f, %b1;"
            "mov %1, %%cr4;"
            : "=r" (ctx->cr4), "=r" (tmp) :: "memory");
    }

    asm volatile(
        "movl %%cr0, %0;"
        "orl $0x40000000, %0;"
        "wbinvd;"
        "movl  %0, %%cr0;"
        "wbinvd;"
        : "=r" (tmp) :: "memory");

    rdmsrll(IA32_MSR_MTRR_DEFTYPE, deftype);
    ctx->deftype = deftype;
    deftype &= ~(IA32_MSR_MTRR_DEFTYPE_E | IA32_MSR_MTRR_DEFTYPE_FE | IA32_MSR_MTRR_DEFTYPE_TYPE_MASK) | CACHE_UC;
    wrmsrll(IA32_MSR_MTRR_DEFTYPE, deftype);
}

static void mtrr_change_exit(mtrr_context_t* ctx)
{
    uintptr_t tmp;

    asm volatile("wbinvd" ::: "memory");

    asm volatile(
        "movl %%cr0, %0;"
        "andl $0xbfffffff, %0;"
        "movl %0, %%cr0;"
        : "=r" (tmp) :: "memory");

    if (cpu_has(X86_FEATURE_PGE))
    {
        asm volatile("mov %0, %%cr4" :: "r" (ctx->cr4));
    }

    irq_restore(ctx->flags);
}

static mtrr_var_entry_t* mtrr_free_entry_find(void)
{
    for (size_t i = 0; i < mtrr_var_size; ++i)
    {
        mtrr_var_entry_t* e = &mtrr_var_entries[i];
        if (!e->valid)
        {
            return e;
        }
    }
    return NULL;
}

static void mtrr_var_write(mtrr_var_entry_t* e)
{
    size_t i = e - mtrr_var_entries;
    uint64_t base = e->type | ((uint64_t)e->paddr);
    uint64_t mask = (~((uint64_t)e->size - 1) & ((1ULL << cpu_info.phys_bits) - 1)) | IA32_MSR_MTRR_PHYSMASK_V;

    log_debug(DEBUG_MTRR, "reg%02d: base: %llx mask: %llx", i, base, mask);

    wrmsrll(IA32_MSR_MTRR_PHYSBASE(i), base);
    wrmsrll(IA32_MSR_MTRR_PHYSMASK(i), mask);
}

static int mtrr_fix_add(uintptr_t paddr, size_t size, cache_policy_t type)
{
    // TODO
    UNUSED(paddr); UNUSED(size); UNUSED(type);
    return -ENOMEM;
}

static int mtrr_var_add(uintptr_t paddr, size_t size, cache_policy_t type)
{
    mtrr_context_t ctx = {};

    if (size < (1 << 17) || (size & ~(size - 1)) - size || (paddr & (size - 1)))
    {
        log_warning("wrong alignment; paddr: %p, size: %p, type: %s",
            paddr, size, cache_policy_string(type));
        return -EINVAL;
    }

    scoped_mutex_lock(&lock);

    mtrr_var_entry_t* e = mtrr_free_entry_find();

    if (unlikely(!e))
    {
        return -ENOMEM;
    }

    e->valid = true;
    e->paddr = paddr;
    e->size  = size;
    e->type  = type;

    log_debug(DEBUG_MTRR, "%p size: %p type: %s", e->paddr, e->size, cache_policy_string(e->type));

    mtrr_change_enter(&ctx);
    mtrr_var_write(e);
    mtrr_change_exit(&ctx);

    return 0;
}

int mtrr_add(uintptr_t paddr, size_t size, cache_policy_t type)
{
    if (unlikely(!mtrr || (type == CACHE_WC && !mtrr_wc)))
    {
        return -ENOSYS;
    }

    if (unlikely(!size || type > CACHE_MAX || type == CACHE_RSVD1 || type == CACHE_RSVD2))
    {
        return -EINVAL;
    }

    return paddr < 0x100000
        ? mtrr_fix_add(paddr, size, type)
        : mtrr_var_add(paddr, size, type);
}

static void mtrr_fix_clear(uint16_t reg)
{
    wrmsrll(reg, 0x0606060606060606ULL);
}

void mtrr_initialize(void)
{
    uint64_t mtrrcap;
    bool disable = false;
    mtrr_context_t ctx = {};

    if (!cpu_has(X86_FEATURE_MSR))
    {
        log_info("no MSR support");
        return;
    }

    if (!cpu_has(X86_FEATURE_MTRR))
    {
        log_info("no MTRR support");
        return;
    }

    if (cpu_info.vendor_id != INTEL)
    {
        log_info("unsupported CPU");
        disable = true;
    }

    if (pat_enabled())
    {
        log_info("PAT available");
        disable = true;
    }

    rdmsrll(IA32_MSR_MTRRCAP, mtrrcap);

    if (mtrrcap & IA32_MSR_MTRRCAP_WC)
    {
        mtrr_wc = true;
    }

    mtrr_var_size = mtrrcap & IA32_MSR_MTRRCAP_VCNT_MASK;

    log_info("variable ranges count: %u; write-combining: %s",
        mtrr_var_size, mtrr_wc ? "true" : "false");

    mtrr_change_enter(&ctx);

    mtrr_fix_clear(IA32_MSR_MTRR_FIX_64K_00000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_16K_80000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_16K_A0000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_C0000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_C8000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_D0000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_D8000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_E0000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_E8000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_F0000);
    mtrr_fix_clear(IA32_MSR_MTRR_FIX_4K_F8000);

    for (size_t i = 0; i < mtrr_var_size; ++i)
    {
        uint64_t base, mask;
        rdmsrll(IA32_MSR_MTRR_PHYSBASE(i), base);
        rdmsrll(IA32_MSR_MTRR_PHYSMASK(i), mask);

        if (mask)
        {
            uint64_t paddr = mtrr_paddr_get(base);
            size_t size = mtrr_size_get(mask);
            log_info("clearing entry %u: [%llx - %llx]", i, paddr, paddr + size - 1);

            wrmsrll(IA32_MSR_MTRR_PHYSMASK(i), 0);
            wrmsrll(IA32_MSR_MTRR_PHYSBASE(i), 0);
        }
    }

    if (disable)
    {
        log_info("disabling MTRR");
        mtrr = false;
        ctx.deftype = CACHE_WB;
    }
    else
    {
        mtrr = true;
        ctx.deftype = IA32_MSR_MTRR_DEFTYPE_E | IA32_MSR_MTRR_DEFTYPE_FE | CACHE_WB;
    }

    mtrr_change_exit(&ctx);

    if (!mtrr)
    {
        return;
    }

    mtrr_var_entries = zalloc_array(mtrr_var_entry_t, mtrr_var_size);

    if (unlikely(!mtrr_var_entries))
    {
        log_warning("cannot allocate mtrr_var_entries");
        mtrr = false;
    }
}
