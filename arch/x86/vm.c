#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/process.h>
#include <kernel/vm_print.h>

int arch_vm_apply(pgd_t* pgd, page_t* pages, uint32_t start, uint32_t end, int vm_flags)
{
    pgt_t* pgt;
    uint32_t paddr, pde_index, pte_index;
    page_t* page = pages;

    for (uint32_t vaddr = start;
         vaddr < end;
         vaddr += PAGE_SIZE, page = list_next_entry(&page->list_entry, page_t, list_entry))
    {
        paddr = page_phys(page);
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        if (!pgd[pde_index])
        {
            pgt = pgt_alloc();

            if (unlikely(!pgt))
            {
                return -ENOMEM;
            }

            log_debug(DEBUG_VM_APPLY, "creating pde[%u]", pde_index);

            pgd[pde_index] = phys_addr(pgt) | pde_flags_get(vm_flags);
        }
        else
        {
            pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
        }

        if (!pgt[pte_index])
        {
            log_debug(DEBUG_VM_APPLY, "mapping v=%x <=> p=%x @ %u:%u pgt=%x",
                vaddr, paddr, pde_index, pte_index, pgt);

            pgt[pte_index] = paddr | pte_flags_get(vm_flags);
        }
        else
        {
            log_debug(DEBUG_VM_APPLY, "mapping exits: v=%x <=> p=%x @ %u:%u pgt=%x",
                vaddr, paddr, pde_index, pte_index, pgt);
        }
    }

    return 0;
}

int arch_vm_reapply(pgd_t* pgd, page_t* pages, uint32_t start, uint32_t end, int vm_flags)
{
    pgt_t* pgt;
    uint32_t paddr, pde_index, pte_index;
    page_t* page = pages;

    for (uint32_t vaddr = start;
         vaddr < end;
         vaddr += PAGE_SIZE, page = list_next_entry(&page->list_entry, page_t, list_entry))
    {
        paddr = page_phys(page);
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);

        log_debug(DEBUG_VM_APPLY, "mapping v=%x <=> p=%x @ %u:%u pgt=%x",
            vaddr, paddr, pde_index, pte_index, pgt);

        pgt[pte_index] = paddr | pte_flags_get(vm_flags);
    }

    return 0;
}

typedef enum
{
    UNMAP,
    DELETE,
} operation_t;

#define vaddr_init(v) \
    ({ \
        pde_index = pde_index(v); \
        pte_index = pte_index(v); \
        pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS); \
        v; \
    })

#define check_pgt() \
    ({ \
        if (unlikely(!pgt[pte_index])) \
        { \
            log_error("unmapped vma at pgt = %x; pgt[%u]", pgt, pte_index); \
        } \
        1; \
    })

#define for_each_vaddr(start, end) \
    for (vaddr = vaddr_init(start); \
        vaddr < end && check_pgt(); \
        vaddr = vaddr_init(vaddr + PAGE_SIZE))

static inline void vm_page_free(const pgt_t* pgt, uint32_t pte_index, bool free_pages)
{
    if (free_pages)
    {
        page_free(virt_ptr(pgt[pte_index] & PAGE_ADDRESS));
    }
}

static inline pgt_t* vm_remove(vm_area_t* vma, pgd_t* pgd, pgt_t* prev_pgt, operation_t operation)
{
    pgt_t* pgt;
    bool free_pages = false;
    uint32_t vaddr, pde_index, pte_index;

    /*vm_area_print(KERN_DEBUG, vma);*/

    if (!--vma->pages->refcount)
    {
        if (!(vma->vm_flags & VM_IO))
        {
            free_pages = true;
        }
        delete(vma->pages);
        vma->pages = NULL;
    }

    for_each_vaddr(vma->start, vma->end)
    {
        vm_page_free(pgt, pte_index, free_pages);
        switch (operation)
        {
            case UNMAP:
                pgt[pte_index] = 0;
                break;
            case DELETE:
                if (prev_pgt != pgt)
                {
                    if (prev_pgt)
                    {
                        log_debug(DEBUG_EXIT, "freeing PGT %x", prev_pgt);
                        page_free(prev_pgt);
                    }
                    prev_pgt = pgt;
                }
        }
    }

    return prev_pgt;
}

int vm_unmap(vm_area_t* vma, pgd_t* pgd)
{
    vm_remove(vma, pgd, NULL, UNMAP);
    return 0;
}

int vm_free(vm_area_t* vma_list, pgd_t* pgd)
{
    vm_area_t* temp;
    pgt_t* prev_pgt = NULL;

    for (vm_area_t* vma = vma_list; vma;)
    {
        prev_pgt = vm_remove(vma, pgd, prev_pgt, DELETE);
        temp = vma;
        vma = vma->next;
        delete(temp);
    }

    if (prev_pgt)
    {
        page_free(prev_pgt);
    }

    return 0;
}

int arch_vm_copy(pgd_t* dest_pgd, const pgd_t* src_pgd,  uint32_t start, uint32_t end)
{
    pgt_t* dest_pgt;
    const pgt_t* src_pgt;
    uint32_t pde_index, pte_index, pgd_flags;
    int prev_pgt_index = -1;

    for (uint32_t vaddr = start; vaddr < end; vaddr += PAGE_SIZE)
    {
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        if ((int)pde_index > prev_pgt_index)
        {
            prev_pgt_index = pde_index;
            src_pgt = virt_ptr(src_pgd[pde_index] & PAGE_ADDRESS);

            log_debug(DEBUG_VM_COPY, "pgd=%x pgd[%u]=%x", src_pgd, pde_index, src_pgd[pde_index]);

            // If there's no pge, we must allocate one
            if (!dest_pgd[pde_index])
            {
                pgd_flags = src_pgd[pde_index] & PAGE_MASK;
                dest_pgt = pgt_alloc();

                if (unlikely(!dest_pgt))
                {
                    goto cannot_allocate;
                }

                dest_pgd[pde_index] = phys_addr(dest_pgt) | pgd_flags;
            }
            else
            {
                dest_pgt = virt_ptr(dest_pgd[pde_index] & PAGE_ADDRESS);
            }
        }

        dest_pgt[pte_index] = src_pgt[pte_index] & ~PTE_WRITEABLE;

        log_debug(DEBUG_VM_COPY, "pgt=%x pgt[%u]=%x", dest_pgt, pte_index, src_pgt[pte_index] & ~PTE_WRITEABLE);
    }

    return 0;

cannot_allocate:
    return -ENOMEM;
}

int arch_vm_copy_on_write(vm_area_t* vma, const pgd_t* pgd, page_t* page_range)
{
    pgt_t* pgt;
    page_t* page = page_range;
    uint32_t pde_index, pte_index, pg_flags;

    for (uint32_t vaddr = vma->start;
        vaddr < vma->end;
        vaddr += PAGE_SIZE, page = list_next_entry(&page->list_entry, page_t, list_entry))
    {
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);
        pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
        pg_flags = pgt[pte_index] & PAGE_MASK;

        memcpy(page_virt_ptr(page), ptr(vaddr), PAGE_SIZE);

        log_debug(DEBUG_VM_COW, "setting pgt[%u] = %x", pte_index, page_phys(page) | pg_flags);

        pgt[pte_index] = page_phys(page) | pg_flags | PTE_WRITEABLE;
    }

    pgd_reload();

    return 0;
}

int vm_io_apply(vm_area_t* vma, pgd_t* pgd, uint32_t paddr)
{
    pgt_t* pgt;
    uint32_t vaddr, pde_index, pte_index, pte_flags = pte_flags_get(vma->vm_flags);

    for (vaddr = vma->start;
         vaddr < vma->end;
         vaddr += PAGE_SIZE, paddr += PAGE_SIZE)
    {
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        if (!pgd[pde_index])
        {
            pgt = pgt_alloc();
            if (unlikely(!pgt))
            {
                return -ENOMEM;
            }

            pgd[pde_index] = phys_addr(pgt) | PDE_PRESENT | PDE_WRITEABLE | PDE_USER;
        }
        else
        {
            pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
        }

        log_debug(DEBUG_VM_APPLY, "mapping v=%x <=> p=%x @ %u:%u pgt=%x",
            vaddr, paddr, pde_index, pte_index, pgt);

        pgt[pte_index] = paddr | pte_flags | PTE_CACHEDIS;
    }

    vma->vm_flags |= VM_IO;

    return 0;
}
