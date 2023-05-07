#pragma once

struct pages;
struct vm_area;
typedef struct pages pages_t;
typedef struct vm_area vm_area_t;

static inline uint32_t pde_flags_get(int)
{
    uint32_t pgt_flags = PDE_PRESENT | PDE_USER | PDE_WRITEABLE;
    return pgt_flags;
}

static inline uint32_t pte_flags_get(int vm_flags)
{
    uint32_t pg_flags = PTE_PRESENT | PTE_USER;
    if (vm_flags & VM_WRITE) pg_flags |= PTE_WRITEABLE;
    return pg_flags;
}

static inline uint32_t vm_paddr(uint32_t vaddr, const pgd_t* pgd)
{
    uint32_t offset = vaddr - page_beginning(vaddr);
    uint32_t pde_index = pde_index(vaddr);
    uint32_t pte_index = pte_index(vaddr);
    if (!pgd[pde_index])
    {
        return 0;
    }

    pgt_t* pgt = virt_ptr(pgd[pde_index] & ~PAGE_MASK);
    return (pgt[pte_index] & ~PAGE_MASK) + offset;
}

static inline uint32_t vm_paddr_end(vm_area_t* vma, pgd_t* pgd)
{
    uint32_t vaddr = page_beginning(vma->end - 1);
    uint32_t pde_index = pde_index(vaddr);
    uint32_t pte_index = pte_index(vaddr);
    if (!pgd[pde_index])
    {
        return 0;
    }

    pgt_t* pgt = virt_ptr(pgd[pde_index] & ~PAGE_MASK);
    return (pgt[pte_index] & ~PAGE_MASK) + PAGE_SIZE;
}
