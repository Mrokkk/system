#include <arch/vm.h>
#include <arch/page.h>

#include <kernel/process.h>

vm_area_t* vm_create(
    page_t* page_range,
    uint32_t virt_address,
    uint32_t size,
    int vm_flags)
{
    vm_area_t* area = alloc(vm_area_t);

    if (unlikely(!area))
    {
        log_debug("no mem");
        return NULL;
    }

    area->pages = alloc(pages_t);
    area->pages->count = 1;
    list_init(&area->pages->head);
    list_merge(&area->pages->head, &page_range->list_entry);
    area->virt_address = virt_address;
    area->size = size;
    area->vm_flags = vm_flags;
    area->next = NULL;

    return area;
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

vm_area_t* vm_extend(
    vm_area_t* area,
    page_t* page_range,
    int vm_flags,
    int count)
{
    log_debug("page_range=%x paddr=%x vaddr=%x count=%u",
        page_range,
        page_phys(page_range),
        page_virt(page_range),
        count);

    area->size += count * PAGE_SIZE;
    area->vm_flags |= vm_flags;
    list_merge(&area->pages->head, &page_range->list_entry);
    return area;
}

int vm_apply(
    vm_area_t* area,
    pgd_t* pgd,
    int vm_apply_flags)
{
    uint32_t paddress;
    uint32_t vaddress = area->virt_address;
    size_t size = area->size;
    int vm_flags = area->vm_flags;
    int replace_pg = vm_apply_flags & VM_APPLY_REPLACE_PG;

    pgt_t* pgt;
    uint32_t pde_index = pde_index(vaddress);
    uint32_t pte_index;

    log_debug("with v=%x, size=%x, pgd=%x",
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
        log_debug("creating pde at offset %u", pde_index);
        uint32_t frame = phys_addr(pgt);

        pgd[pde_index] = frame | pde_flags_get(vm_flags);
    }
    else
    {
        pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);
        log_debug("pde exists; %x", pgt);
    }

    log_debug("pgt=%x", pgt);

    page_t* page = list_front(&area->pages->head, page_t, list_entry);
    for (uint32_t vaddr = vaddress;
         vaddr < vaddress + size;
         vaddr += PAGE_SIZE, page = list_next_entry(&page->list_entry, page_t, list_entry))
    {
        paddress = page_phys(page);
        pte_index = pte_index(vaddr);

        log_debug("PTE entry at offset %u", pte_index);
        if (!pgt[pte_index] || replace_pg)
        {
            log_debug("mapping vaddress=%x <=> paddress=%x", vaddr, paddress);
            pgt[pte_index] = paddress | pte_flags_get(vm_flags);
        }
        else
        {
            log_debug("mapping already exits: vaddress=%x <=> paddress=%x", vaddr, paddress);
        }
    }

    return 0;
}

int vm_remove(
    vm_area_t* area,
    pgd_t* pgd,
    int remove_pte)
{
    uint32_t vaddress = area->virt_address;
    size_t size = area->size;

    pgt_t* pgt;
    uint32_t pde_index = pde_index(vaddress);
    uint32_t pte_index;

#if TRACE_VM
    vm_print_single(area);
#endif

    pgt = virt_ptr(pgd[pde_index] & PAGE_ADDRESS);

    if (!--area->pages->count)
    {
        if (!(area->vm_flags & VM_NONFREEABLE))
        {
            page_range_free(&area->pages->head);
        }
        delete(area->pages);
        area->pages = NULL;
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
            log_exception("unmapped area at pgt = %x; pgt[%u]", pgt, pgd);
        }
        pgt[pte_index] = 0;
    }
    return 0;
}

int vm_copy(
    vm_area_t* dest_area,
    const vm_area_t* src_area,
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
    memcpy(dest_area, src_area, sizeof(vm_area_t) - sizeof(struct vm_area*));
    dest_area->next = NULL;
    dest_area->pages->count++;

    for (uint32_t vaddr = src_area->virt_address; vaddr < vm_virt_end(src_area); vaddr += PAGE_SIZE)
    {
        pde_index = pde_index(vaddr);
        pte_index = pte_index(vaddr);

        // Get pge
        if ((int)pde_index > prev_pgt_index)
        {
            prev_pgt_index = pde_index;
            src_pgt = virt_ptr(src_pgd[pde_index] & PAGE_ADDRESS);

            log_debug("pgd=%x pgd[%u]=%x", src_pgd, pde_index, src_pgd[pde_index]);

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

        log_debug("pgt=%x pgt[%u]=%x", dest_pgt, pte_index, src_pgt[pte_index] & ~PTE_WRITEABLE);
    }

    return 0;

cannot_allocate:
    return -ENOMEM;
}

static inline int address_within(uint32_t virt_address, vm_area_t* area)
{
    return virt_address >= area->virt_address && virt_address < vm_virt_end(area);
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

void vm_print(const vm_area_t* area)
{
    if (!area)
    {
        return;
    }
    for (const vm_area_t* temp = area; temp; temp = temp->next)
    {
        vm_print_single(temp);
    }
}

int copy_on_write(vm_area_t* area)
{
    const pgd_t* pgd = ptr(process_current->mm->pgd);
    uint32_t pde_index = pde_index(area->virt_address);
    pgt_t* pgt_ptr = virt_ptr(pgd[pde_index] & ~PAGE_MASK);

    if (!--area->pages->count)
    {
        panic("cow on not shared pages!");
    }

    page_t* page_range = page_alloc(area->size / PAGE_SIZE, PAGE_ALLOC_CONT);

    if (unlikely(!page_range))
    {
        return -1;
    }

    area->pages = alloc(pages_t);
    area->pages->count = 1;
    list_init(&area->pages->head);
    list_merge(&area->pages->head, &page_range->list_entry);

    memcpy(page_virt_ptr(page_range), ptr(area->virt_address), area->size);

    uint32_t new_phys_address = page_phys(page_range);
    for (uint32_t old_vaddr = area->virt_address; old_vaddr < vm_virt_end(area); old_vaddr += PAGE_SIZE, new_phys_address += PAGE_SIZE)
    {
        uint32_t pte_index = pte_index(old_vaddr);
        uint32_t pg = pgt_ptr[pte_index];
        uint32_t pg_flags = (pg & PAGE_MASK) | PTE_WRITEABLE;

        log_debug("setting pgt[%u] = %x", pte_index, new_phys_address | pg_flags);

        pgt_ptr[pte_index] = new_phys_address | pg_flags;
    }

    area->vm_flags &= ~VM_NONFREEABLE;

    pgd_reload();

    return 0;
}
