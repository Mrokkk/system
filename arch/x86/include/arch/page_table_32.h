#pragma once

#ifndef __ASSEMBLER__

#include <stdint.h>
#include <kernel/cpu.h>
#include <kernel/minmax.h>
#include <kernel/compiler.h>
#include <arch/page_types.h>

static inline int pud_alloc_impl(pgd_t*, uintptr_t)
{
    return 0;
}

static inline int pmd_alloc_impl(pud_t*, uintptr_t)
{
    return 0;
}

int pte_alloc_impl(pmd_t* pmd);

#define PAGE_TABLES 2

#define PGD_SHIFT 22
#define PUD_SHIFT PGD_SHIFT
#define PMD_SHIFT PUD_SHIFT
#define PGT_SHIFT 12

#define PGD_ENTRY_SIZE  (1 << PGD_SHIFT)
#define PGT_ENTRY_SIZE  (1 << PGT_SHIFT)

#define PUD_FOLDED
#define PMD_FOLDED

#define PTRS_PER_PGD    1
#define PTRS_PER_PUD    1
#define PTRS_PER_PMD    1024
#define PTRS_PER_PTE    1024

#define KERNEL_PGD_OFFSET       (pmd_index(KERNEL_PAGE_OFFSET))

#define __page_nop()            do { } while (0)

#define pgd_index(vaddr)        ((vaddr) >> PGD_SHIFT)
#define pud_index(vaddr)        (((vaddr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))
#define pmd_index(vaddr)        (((vaddr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))
#define pte_index(vaddr)        (((vaddr) >> PGT_SHIFT) & (PTRS_PER_PTE - 1))

#define pgd_entry_none(pgd)     ({ 0; })
#define pud_entry_none(pud)     ({ 0; })
#define pmd_entry_none(pmd)     ({ !*(pmd); })
#define pte_entry_none(pte)     ({ !*(pte); })

#define pgd_addr_next(addr, end) ({ min(align(addr + 1, PGD_ENTRY_SIZE), end); })
#define pud_addr_next(addr, end) ({ end; })
#define pmd_addr_next(addr, end) ({ end; })
#define pte_addr_next(addr, end) ({ min(addr + PAGE_SIZE, end); })

#define pgd_entry_page(pgd) NULL
#define pud_entry_page(pud) NULL
#define pmd_entry_page(pmd) page(*(pmd) & PAGE_ADDRESS)
#define pte_entry_page(pte) page(*(pte) & PAGE_ADDRESS)

#define pte_entry_paddr(pte) ({ *(pte) & PAGE_ADDRESS; })
#define pte_entry_prot(pte) ({ *(pte) & PAGE_MASK; })

#define __pgtable_page(entry) ({ page(*(entry) & PAGE_ADDRESS); })
#define __pgtable_phys(entry) ({ *(entry) & PAGE_ADDRESS; })
#define __pgtable_virt(entry) ({ virt(*(entry) & PAGE_ADDRESS); })

#define pgd_offset(pgd, vaddr)  ({ (pgd) + pgd_index(vaddr); })
#define pud_offset(pgd, vaddr)  ({ (pud_t*)(pgd); })
#define pmd_offset(pud, vaddr)  ({ (pmd_t*)(pud); })
#define pte_offset(pmd, vaddr)  ((pte_t*)(__pgtable_virt(pmd)) + pte_index(vaddr))

#define pgd_free(pgd) ({ page_free(pgd); })
#define pud_free(pgd) __page_nop()
#define pmd_free(pud) __page_nop()
#define pte_free(pmd) ({ pages_free(__pgtable_page(pmd)); })

#define pgd_entry_clear(pgd) __page_nop()
#define pud_entry_clear(pud) __page_nop()
#define pmd_entry_clear(pmd) ({ *(pmd) = 0; })
#define pte_entry_clear(pte) ({ *(pte) = 0; })

#define pgd_entry_set(pgd, paddr, pgprot) __page_nop()
#define pud_entry_set(pud, paddr, pgprot) __page_nop()
#define pmd_entry_set(pmd, paddr, pgprot) ({ *(pmd) = (paddr) | (pgprot); })
#define pte_entry_set(pte, paddr, pgprot) ({ *(pte) = (paddr) | (pgprot); })

#define pte_entry_prot_set(pte, pgprot) ({ *(pte) &= ~PAGE_MASK; *(pte) |= pgprot; })
#define pte_entry_cow(dst, from)        ({ *(dst) = *(from) &= ~PAGE_RW; })

#endif // __ASSEMBLER__
