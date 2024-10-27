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

#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12

#define PGD_ENTRY_SIZE  (1 << PGD_SHIFT)
#define PUD_ENTRY_SIZE  (1 << PUD_SHIFT)
#define PMD_ENTRY_SIZE  (1 << PMD_SHIFT)
#define PTE_ENTRY_SIZE  (1 << PTE_SHIFT)

#define PTRS_PER_PGD 512
#define PTRS_PER_PUD 512
#define PTRS_PER_PMD 512
#define PTRS_PER_PGT 512

#define pgd_index(vaddr)        (((vaddr) >> PGD_SHIFT) & (PTRS_PER_PGD - 1))
#define pud_index(vaddr)        (((vaddr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))
#define pmd_index(vaddr)        (((vaddr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))
#define pte_index(vaddr)        (((vaddr) >> PTE_SHIFT) & (PTRS_PER_PGT - 1))

#define PGD_OFFSET              (pgd_index(KERNEL_PAGE_OFFSET))

#define pgd_entry_none(pgd) ({ !*(pgd); })
#define pud_entry_none(pud) ({ !*(pud); })
#define pmd_entry_none(pmd) ({ !*(pmd); })
#define pte_entry_none(pte) ({ !*(pte); })

#define pgd_addr_next(addr, end) ({ min(align(addr + 1, PGD_ENTRY_SIZE), end); })
#define pud_addr_next(addr, end) ({ min(align(addr + 1, PUD_ENTRY_SIZE), end); })
#define pmd_addr_next(addr, end) ({ min(align(addr + 1, PMD_ENTRY_SIZE), end); })
#define pte_addr_next(addr, end) ({ min(addr + PAGE_SIZE, end); })

#define __pgtable_page(entry) page(*(entry) & PAGE_ADDRESS)
#define __pgtable_virt_ptr(entry) virt(*(entry) & PAGE_ADDRESS)

#define pgd_entry_page(pgd) page(*(pgd) & PAGE_ADDRESS)
#define pud_entry_page(pud) page(*(pud) & PAGE_ADDRESS)
#define pmd_entry_page(pmd) page(*(pmd) & PAGE_ADDRESS)
#define pte_entry_page(pte) page(*(pte) & PAGE_ADDRESS)

#define pgd_offset(pgd, vaddr)  ({ (pgd) + pgd_index(vaddr); })
#define pud_offset(pgd, vaddr)  ({ (pud_t*)(__pgtable_virt_ptr(pgd)) + pud_index(vaddr); })
#define pmd_offset(pud, vaddr)  ({ (pmd_t*)(__pgtable_virt_ptr(pud)) + pmd_index(vaddr); })
#define pte_offset(pmd, vaddr)  ({ (pte_t*)(__pgtable_virt_ptr(pmd)) + pte_index(vaddr); })

#define pgd_free(pgd) ({ page_free(pgd); })
#define pud_free(pgd) ({ page_free(__pgtable_page(pgd)); })
#define pmd_free(pud) ({ page_free(__pgtable_page(pud)); })
#define pte_free(pmd) ({ pages_free(__pgtable_page(pmd)); })

#define pgd_entry_clear(pgd) ({ *(pgd) = 0; })
#define pud_entry_clear(pud) ({ *(pud) = 0; })
#define pmd_entry_clear(pmd) ({ *(pmd) = 0; })
#define pte_entry_clear(pte) ({ *(pte) = 0; })

#define pgd_entry_set(pgd, paddr, pgprot) ({ *(pgd) = (paddr) | (pgprot); })
#define pud_entry_set(pud, paddr, pgprot) ({ *(pud) = (paddr) | (pgprot); })
#define pmd_entry_set(pmd, paddr, pgprot) ({ *(pmd) = (paddr) | (pgprot); })
#define pte_entry_set(pte, paddr, pgprot) ({ *(pte) = (paddr) | (pgprot); })

#define pte_entry_pgprot_set(pte, pgprot) ({ *(pte) &= ~PAGE_MASK; *(pte) |= pgprot; })
#define pte_entry_cow(dst, from) ({ *(dst) = *(from) &= ~PAGE_RW; })

#endif // __ASSEMBLER__
