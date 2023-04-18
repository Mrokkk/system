#include <arch/vm.h>

#include <kernel/page.h>
#include <kernel/process.h>

vm_area_t* vm_create(page_t* page_range, uint32_t virt_address, uint32_t size, int vm_flags)
{
    vm_area_t* vma = alloc(vm_area_t);

    if (unlikely(!vma))
    {
        log_debug(DEBUG_VM, "cannot allocate vm_area");
        return NULL;
    }

    vma->pages = alloc(pages_t);

    if (unlikely(!vma->pages))
    {
        log_debug(DEBUG_VM, "cannot allocate vma->pages");
        return NULL;
    }

    vma->pages->count = 1;
    list_init(&vma->pages->head);
    list_merge(&vma->pages->head, &page_range->list_entry);
    vma->virt_address = virt_address;
    vma->size = size;
    vma->vm_flags = vm_flags;
    vma->next = NULL;

    return vma;
}

int vm_add(vm_area_t** head, vm_area_t* new_vma)
{
    uint32_t new_end = vm_virt_end(new_vma);
    uint32_t new_start = new_vma->virt_address;

    if (!*head)
    {
        *head = new_vma;
        return 0;
    }

    if ((*head)->virt_address >= new_end)
    {
        new_vma->next = *head;
        *head = new_vma;
        return 0;
    }

    vm_area_t* next;
    vm_area_t* prev = NULL;

    for (vm_area_t* temp = *head; temp; prev = temp, temp = temp->next)
    {
        if (temp->next && vm_virt_end(temp) <= new_start && temp->next->virt_address >= new_end)
        {
            next = temp->next;
            temp->next = new_vma;
            new_vma->next = next;
            return 0;
        }
    }

    prev->next = new_vma;
    new_vma->next = NULL;

    return 0;
}

vm_area_t* vm_extend(vm_area_t* vma, page_t* page_range, int vm_flags, int count)
{
    log_debug(DEBUG_VM, "page_range=%x paddr=%x vaddr=%x count=%u",
        page_range,
        page_phys(page_range),
        page_virt(page_range),
        count);

    vma->size += count * PAGE_SIZE;
    vma->vm_flags |= vm_flags;
    list_merge(&vma->pages->head, &page_range->list_entry);
    return vma;
}

int vm_apply(vm_area_t* vma, pgd_t* pgd, int vm_apply_flags)
{
    uint32_t paddress;
    uint32_t vaddress = vma->virt_address;
    size_t size = vma->size;
    int vm_flags = vma->vm_flags;
    int replace_pg = vm_apply_flags & VM_APPLY_REPLACE_PG;

    pgt_t* pgt;
    uint32_t pde_index = pde_index(vaddress);
    uint32_t pte_index;

    log_debug(DEBUG_VM_APPLY,
        "with v=%x, size=%x, pgd=%x",
        vaddress,
        size,
        pgd);

    if (size >= PAGES_IN_PTE * PAGE_SIZE)
    {
        // TODO: add support for 4M areas
        return -EFAULT;
    }

    if (!pgd[pde_index])
    {
        pgt = single_page();

        if (unlikely(!pgt))
        {
            return -ENOMEM;
        }

        memset(pgt, 0, PAGE_SIZE);
        log_debug(DEBUG_VM_APPLY, "creating pde at offset %u", pde_index);
        uint32_t frame = phys_addr(pgt);

        pgd[pde_index] = frame | pde_flags_get(vm_flags);
    }
    else
    {
        pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
        log_debug(DEBUG_VM_APPLY, "pde exists; %x", pgt);
    }

    log_debug(DEBUG_VM_APPLY, "pgt=%x", pgt);

    page_t* page = list_front(&vma->pages->head, page_t, list_entry);
    for (uint32_t vaddr = vaddress;
         vaddr < vaddress + size;
         vaddr += PAGE_SIZE, page = list_next_entry(&page->list_entry, page_t, list_entry))
    {
        paddress = page_phys(page);
        pte_index = pte_index(vaddr);

        log_debug(DEBUG_VM_APPLY, "PTE entry at offset %u", pte_index);
        if (!pgt[pte_index] || replace_pg)
        {
            log_debug(DEBUG_VM_APPLY, "mapping vaddress=%x <=> paddress=%x", vaddr, paddress);
            pgt[pte_index] = paddress | pte_flags_get(vm_flags);
        }
        else
        {
            log_debug(DEBUG_VM_APPLY, "mapping already exits: vaddress=%x <=> paddress=%x", vaddr, paddress);
        }
    }

    return 0;
}

int vm_remove(vm_area_t* vma, pgd_t* pgd, int remove_pte)
{
    uint32_t vaddress = vma->virt_address;
    size_t size = vma->size;

    pgt_t* pgt;
    uint32_t pde_index = pde_index(vaddress);
    uint32_t pte_index;

    vm_print_single(vma, DEBUG_VM);

    pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);

    if (!--vma->pages->count)
    {
        if (!(vma->vm_flags & VM_NONFREEABLE))
        {
            page_range_free(&vma->pages->head);
        }
        delete(vma->pages);
        vma->pages = NULL;
    }

    if (remove_pte)
    {
        goto remove_table;
    }

    return 0;

remove_table:
    for (uint32_t vaddr = vaddress; vaddr < vaddress + size; vaddr += PAGE_SIZE)
    {
        pte_index = pte_index(vaddr);
        if (unlikely(!pgt[pte_index]))
        {
            log_error("unmapped vma at pgt = %x; pgt[%u]", pgt, pgd);
        }
        pgt[pte_index] = 0;
    }
    return 0;
}

int vm_copy(
    vm_area_t* dest_vma,
    const vm_area_t* src_vma,
    pgd_t* dest_pgd,
    const pgd_t* src_pgd)
{
    uint32_t pde_index;
    uint32_t pte_index;
    pgt_t* dest_pgt;
    pgt_t* src_pgt;
    uint32_t pgd_flags;
    int prev_pgt_index = -1;

    // Copy vm_area except for next
    memcpy(dest_vma, src_vma, sizeof(vm_area_t) - sizeof(struct vm_area*));
    dest_vma->next = NULL;
    dest_vma->pages->count++;

    for (uint32_t vaddr = src_vma->virt_address; vaddr < vm_virt_end(src_vma); vaddr += PAGE_SIZE)
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

static inline int address_within(uint32_t virt_address, vm_area_t* vma)
{
    return virt_address >= vma->virt_address && virt_address < vm_virt_end(vma);
}

vm_area_t* vm_find(
    uint32_t virt_address,
    vm_area_t* areas)
{
    vm_area_t* temp;
    for (temp = areas; temp; temp = temp->next)
    {
        if (address_within(virt_address, temp))
        {
            return temp;
        }
    }
    return NULL;
}

void vm_print(const vm_area_t* vma)
{
    if (!vma)
    {
        return;
    }
    for (const vm_area_t* temp = vma; temp; temp = temp->next)
    {
        vm_print_single(temp, DEBUG_VM);
    }
}

int vm_copy_on_write(vm_area_t* vma)
{
    const pgd_t* pgd = ptr(process_current->mm->pgd);
    uint32_t pde_index = pde_index(vma->virt_address);
    pgt_t* pgt = virt_ptr(pgd[pde_index] & ~PAGE_MASK);

    vm_print_single(vma, DEBUG_VM_COW);

    if (!--vma->pages->count)
    {
        panic("cow on not shared pages!");
    }

    page_t* page_range = page_alloc(vma->size / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!page_range))
    {
        log_debug(DEBUG_VM_COW, "cannot allocate pages");
        return -ENOMEM;
    }

    vma->pages = alloc(pages_t);

    if (unlikely(!vma->pages))
    {
        log_debug(DEBUG_VM_COW, "cannot allocate vma->pages");
        return -ENOMEM;
    }

    vma->pages->count = 1;
    list_init(&vma->pages->head);
    list_merge(&vma->pages->head, &page_range->list_entry);

    memcpy(page_virt_ptr(page_range), ptr(vma->virt_address), vma->size);

    uint32_t new_phys_address = page_phys(page_range);
    for (uint32_t old_vaddr = vma->virt_address; old_vaddr < vm_virt_end(vma); old_vaddr += PAGE_SIZE, new_phys_address += PAGE_SIZE)
    {
        uint32_t pte_index = pte_index(old_vaddr);
        uint32_t pg = pgt[pte_index];
        uint32_t pg_flags = (pg & PAGE_MASK) | PTE_WRITEABLE;

        log_debug(DEBUG_VM_COW, "setting pgt[%u] = %x", pte_index, new_phys_address | pg_flags);

        pgt[pte_index] = new_phys_address | pg_flags;
    }

    vma->vm_flags &= ~VM_NONFREEABLE;

    pgd_reload();

    return 0;
}

int __vm_verify(char verify, uint32_t vaddr, size_t size, vm_area_t* vma)
{
    for (; vma; vma = vma->next)
    {
        if (vaddr >= vma->virt_address && (vaddr + size) <= (vma->virt_address + vma->size))
        {
            if (verify == VERIFY_WRITE)
            {
                return vma->vm_flags & VM_WRITE
                    ? 0
                    : -EFAULT;
            }

            return vma->vm_flags & VM_READ
                ? 0
                : -EFAULT;
        }
    }

    return -EFAULT;
}
