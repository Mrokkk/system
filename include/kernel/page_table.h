#pragma once

#include <arch/page_table.h>
#include <kernel/page_types.h>

MUST_CHECK(pgd_t*) pgd_alloc(void);

static inline pud_t* pud_alloc(pgd_t* pgd, uintptr_t vaddr)
{
    return (unlikely(pgd_entry_none(pgd) && pud_alloc_impl(pgd, vaddr)))
        ? NULL
        : pud_offset(pgd, vaddr);
}

static inline pmd_t* pmd_alloc(pud_t* pud, uintptr_t vaddr)
{
    return (unlikely(pud_entry_none(pud) && pmd_alloc_impl(pud, vaddr)))
        ? NULL
        : pmd_offset(pud, vaddr);
}

static inline pte_t* pte_alloc(pmd_t* pmd, uintptr_t vaddr)
{
    return (unlikely(pmd_entry_none(pmd) && pte_alloc_impl(pmd)))
        ? NULL
        : pte_offset(pmd, vaddr);
}

extern pgd_t kernel_page_dir[];
