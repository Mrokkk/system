#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/segmexec.h>
#include <kernel/vm_print.h>

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

#define for_each_vaddr(start, end) \
    for (vaddr = vaddr_init(start); \
        vaddr < end; \
        vaddr = vaddr_init(vaddr + PAGE_SIZE))

static void vm_remove_range_impl(
    vm_area_t* vma,
    uintptr_t start,
    uintptr_t end,
    pgd_t* pgd,
    operation_t operation)
{
    pgt_t* pgt;
    bool free_pages = false;
    uintptr_t vaddr, pde_index, pte_index;

    scoped_irq_lock();

    if (!(vma->vm_flags & VM_IO))
    {
        free_pages = true;
    }

    for_each_vaddr(start, end)
    {
        if (unlikely(!pgd[pde_index]))
        {
            continue;
        }

        if (pgt[pte_index] && free_pages)
        {
            uintptr_t paddr = pgt[pte_index] & PAGE_ADDRESS;
            page_t* p = page(paddr);

            pages_free(p);
        }

        switch (operation)
        {
            case UNMAP:
                pgt[pte_index] = 0;
                break;
            case DELETE: // We don't need to clear the pte
        }
    }
}

int vm_unmap_range(vm_area_t* vma, uintptr_t start, uintptr_t end, pgd_t* pgd)
{
    vm_remove_range_impl(vma, start, end, pgd, UNMAP);
    return 0;
}

int vm_unmap(vm_area_t* vma, pgd_t* pgd)
{
    vm_remove_range_impl(vma, vma->start, vma->end, pgd, UNMAP);
    return 0;
}

int vm_free(vm_area_t* vma_list, pgd_t* pgd)
{
    vm_area_t* temp;

    for (vm_area_t* vma = vma_list; vma;)
    {
        vm_remove_range_impl(vma, vma->start, vma->end, pgd, DELETE);
        temp = vma;
        vma = vma->next;
        delete(temp);
    }

    for (uintptr_t pde_index = 0; pde_index < KERNEL_PDE_OFFSET; ++pde_index)
    {
        if (pgd[pde_index])
        {
            page_free(virt_ptr(pgd[pde_index] & PAGE_ADDRESS));
        }
    }

    return 0;
}

static pgt_t* pgt_get(pgd_t* pgd, uintptr_t pde_index, int vm_flags)
{
    pgt_t* pgt;

    if (!pgd[pde_index])
    {
        pgt = pgt_alloc();

        if (unlikely(!pgt))
        {
            return NULL;
        }

        pgd[pde_index] = phys_addr(pgt) | pde_flags_get(vm_flags);
    }
    else
    {
        pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
    }

    return pgt;
}

int arch_vm_map_single(pgd_t* pgd, uintptr_t pde_index, uintptr_t pte_index, page_t* page, int vm_flags)
{
    pgt_t* pgt = pgt_get(pgd, pde_index, vm_flags);

    if (unlikely(!pgt))
    {
        return -ENOMEM;
    }

    pgt[pte_index] = page_phys(page) | pte_flags_get(vm_flags);

    return 0;
}

static int arch_vm_copy_impl(pgd_t* dest_pgd, pgd_t* src_pgd, uintptr_t start, uintptr_t end)
{
    pgt_t* dest_pgt;
    pgt_t* src_pgt = NULL;
    uintptr_t pde_index, pte_index, pgd_flags;
    int prev_pde_index = -1;
    uintptr_t src_pgt_paddr;

    for (uintptr_t vaddr = start; vaddr < end; vaddr += PAGE_SIZE)
    {
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        if ((int)pde_index > prev_pde_index)
        {
            prev_pde_index = pde_index;
            src_pgt_paddr = src_pgd[pde_index] & PAGE_ADDRESS;

            if (!src_pgt_paddr)
            {
                continue;
            }

            src_pgt = virt_ptr(src_pgt_paddr);

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

        // FIXME: static analyzer from gcc isn't clever enough to
        // figure out that src_pgt is never accessed uninitialized
        if (src_pgt && src_pgt[pte_index]
#if CONFIG_SEGMEXEC
                && start < CODE_START
#endif
            )
        {
            uintptr_t paddr = src_pgt[pte_index] & PAGE_ADDRESS;

            dest_pgt[pte_index] = src_pgt[pte_index] &= ~PTE_WRITEABLE;
            page(paddr)->refcount++;
        }

        log_debug(DEBUG_VM_COPY, "pgt=%x pgt[%u]=%x", dest_pgt, pte_index, src_pgt[pte_index] & ~PTE_WRITEABLE);
    }

    return 0;

cannot_allocate:
    return -ENOMEM;
}

int arch_vm_copy(vm_area_t* dest_vma, pgd_t* dest_pgd, pgd_t* src_pgd, uintptr_t start, uintptr_t end)
{
    int errno = arch_vm_copy_impl(dest_pgd, src_pgd, start, end);
    UNUSED(dest_vma);

#if CONFIG_SEGMEXEC
    if (unlikely(errno))
    {
        return errno;
    }

    if (dest_vma->vm_flags & VM_EXEC)
    {
        errno = arch_vm_copy_impl(dest_pgd, src_pgd, start + CODE_START, end + CODE_START);
    }
#endif

    return errno;
}

int vm_apply(vm_area_t* vmas, pgd_t* pgd, uintptr_t vaddr_start, uintptr_t vaddr_end)
{
    pgt_t* pgt;
    vm_area_t* vma = vmas;
    uintptr_t vaddr, pde_index, pte_index, pte_flags = pte_flags_get(vmas->vm_flags);

    for (vaddr = vaddr_start; vaddr < vaddr_end; vaddr += PAGE_SIZE)
    {
        while (vaddr >= vma->end)
        {
            vma = vma->next;

            if (unlikely(!vma))
            {
                log_error("%s:%u:%s: cannot apply vm; vma->next is null", __FILE__, __LINE__, __func__);
                vm_area_log(KERN_ERR, vmas);
                return -1;
            }

            pte_flags = pte_flags_get(vma->vm_flags);
        }

        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        if (!(pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS)))
        {
            continue;
        }

        if (!pgt[pte_index])
        {
            continue;
        }

        pgt[pte_index] = (pgt[pte_index] & PAGE_ADDRESS) | pte_flags;
    }

    return 0;
}

int vm_io_apply(vm_area_t* vma, pgd_t* pgd, uintptr_t paddr)
{
    pgt_t* pgt;
    uintptr_t vaddr, pde_index, pte_index, pte_flags = pte_flags_get(vma->vm_flags);

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

    pgd_reload();

    return 0;
}
