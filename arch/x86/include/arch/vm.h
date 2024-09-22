#pragma once

#include <kernel/page.h>

struct pages;
struct vm_area;
typedef struct pages pages_t;
typedef struct vm_area vm_area_t;

static inline uintptr_t pde_flags_get(int)
{
    uintptr_t pgt_flags = PDE_PRESENT | PDE_USER | PDE_WRITEABLE;
    return pgt_flags;
}

static inline uintptr_t pte_flags_get(int vm_flags)
{
    uintptr_t pg_flags = PTE_PRESENT | PTE_USER;
    if (vm_flags & VM_WRITE) pg_flags |= PTE_WRITEABLE;
    return pg_flags;
}

static inline uint32_t vm_paddr(uintptr_t vaddr, const pgd_t* pgd)
{
    uintptr_t offset = vaddr & PAGE_MASK;
    uintptr_t pde_index = pde_index(vaddr);
    uintptr_t pte_index = pte_index(vaddr);

    if (!pgd[pde_index])
    {
        return 0;
    }

    pgt_t* pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
    return (pgt[pte_index] & PAGE_ADDRESS) + offset;
}

static inline page_t* vm_page(
    const pgd_t* pgd,
    const uintptr_t vaddr,
    uintptr_t* pde_index,
    uintptr_t* pte_index)
{
    *pde_index = pde_index(vaddr);
    *pte_index = pte_index(vaddr);

    if (!pgd[*pde_index])
    {
        return NULL;
    }

    pgt_t* pgt = virt_ptr(pgd[*pde_index] & PAGE_ADDRESS);

    if (!pgt[*pte_index])
    {
        return NULL;
    }

    return page(pgt[*pte_index] & PAGE_ADDRESS);
}
