#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/vm_print.h>

#define DEBUG_NOPAGE 0

vm_area_t* vm_create(uint32_t vaddr, size_t size, int vm_flags)
{
    vm_area_t* vma = alloc(vm_area_t);

    if (unlikely(!vma))
    {
        log_debug(DEBUG_VM, "cannot allocate vm_area");
        return NULL;
    }

    vma->start = vma->end = vaddr;
    vma->end += size;
    vma->vm_flags = vm_flags;
    vma->dentry = NULL;
    vma->next = NULL;
    vma->prev = NULL;
    vma->offset = 0;

    return vma;
}

int vm_add(vm_area_t** head, vm_area_t* new_vma)
{
    uint32_t new_end = new_vma->end;
    uint32_t new_start = new_vma->start;

    if (!*head)
    {
        *head = new_vma;
        new_vma->prev = NULL;
        return 0;
    }

    if ((*head)->start >= new_end)
    {
        new_vma->next = *head;
        new_vma->prev = NULL;
        (*head)->prev = new_vma;
        *head = new_vma;
        return 0;
    }

    vm_area_t* next;
    vm_area_t* prev = NULL;

    for (vm_area_t* temp = *head; temp; prev = temp, temp = temp->next)
    {
        if (temp->next && temp->end <= new_start && temp->next->start >= new_end)
        {
            next = temp->next;
            temp->next = new_vma;
            new_vma->next = next;
            new_vma->prev = temp;
            next->prev = new_vma;
            return 0;
        }
    }

    prev->next = new_vma;
    new_vma->prev = prev;
    new_vma->next = NULL;

    return 0;
}

void vm_del(vm_area_t* vma)
{
    vma->next->prev = vma->prev;
    vma->prev->next = vma->next;
    delete(vma);
}

int vm_copy(vm_area_t* dest_vma, const vm_area_t* src_vma, pgd_t* dest_pgd, pgd_t* src_pgd)
{
    // Copy vm_area except for next and prev
    memcpy(dest_vma, src_vma, sizeof(vm_area_t) - 2 * sizeof(vm_area_t*));

    dest_vma->next = NULL;
    dest_vma->prev = NULL;

    return arch_vm_copy(dest_pgd, src_pgd, src_vma->start, src_vma->end);
}

int vm_nopage(vm_area_t* vma, pgd_t* pgd, uintptr_t address, bool write)
{
    int errno;
    uint32_t pde_index, pte_index;

    if (unlikely(write && !(vma->vm_flags & VM_WRITE)))
    {
        return -EFAULT;
    }

    scoped_irq_lock();

    log_debug(DEBUG_NOPAGE, "address: %x, vma:", address);
    vm_area_log_debug(DEBUG_NOPAGE, vma);

    address = page_beginning(address);

    page_t* page = vm_page(pgd, address, &pde_index, &pte_index);

    if (vma->dentry && !page)
    {
        if (unlikely(!vma->ops))
        {
            log_warning("no vma ops, cannot map file");
            return -ENOSYS;
        }

        errno = vma->ops->nopage(vma, address, &page);

        if (unlikely(errno))
        {
            log_warning("ops->nopage failed: %d", errno);
            return errno;
        }

        goto map_page;
    }

    if (!page)
    {
        page = page_alloc1();

        if (unlikely(!page))
        {
            log_error("no memory available!");
            return -ENOMEM;
        }

        memset(page_virt_ptr(page), 0, PAGE_SIZE);

        goto map_page;
    }
    else
    {
        if (page->refcount > 1)
        {
            page_t* new_page = page_alloc1();

            if (unlikely(!new_page))
            {
                return -ENOMEM;
            }

            memcpy(page_virt_ptr(new_page), ptr(address), PAGE_SIZE);

            page->refcount--;
            page = new_page;
        }
        else if (!write)
        {
            return -EFAULT;
        }
    }

map_page:
    if (unlikely(errno = arch_vm_map_single(pgd, pde_index, pte_index, page, vma->vm_flags)))
    {
        log_warning("arch_vm_map_single failed with %d", errno);
        return errno;
    }

    pgd_reload();

    return 0;
}

static inline bool address_within(uint32_t vaddr, vm_area_t* vma)
{
    return vaddr >= vma->start && vaddr < vma->end;
}

vm_area_t* vm_find(uint32_t vaddr, vm_area_t* vmas)
{
    for (vm_area_t* temp = vmas; temp; temp = temp->next)
    {
        if (address_within(vaddr, temp))
        {
            return temp;
        }
    }
    return NULL;
}

int vm_verify_impl(vm_verify_flag_t verify, uint32_t vaddr, size_t size, vm_area_t* vma)
{
    for (; vma; vma = vma->next)
    {
        if (vaddr >= vma->start && (vaddr + size) <= vma->end)
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

static int vm_verify_string_impl(vm_verify_flag_t flag, const char* string, size_t limit, vm_area_t* vma)
{
    for (; vma; vma = vma->next)
    {
        if (addr(string) >= vma->start && addr(string) < vma->end)
        {
            if (unlikely((flag == VERIFY_WRITE && !(vma->vm_flags & VM_WRITE)) ||
                (flag == VERIFY_READ && !(vma->vm_flags & VM_READ))))
            {
                return -EFAULT;
            }

            size_t i;
            size_t max_len = vma->end - addr(string);

            if (limit < max_len)
            {
                return 0;
            }

            for (i = 1, string++;; ++i, ++string)
            {
                if (unlikely(i > max_len))
                {
                    return -EFAULT;
                }
                if (*string == 0)
                {
                    return 0;
                }
            }
        }
    }

    return -EFAULT;

}

int vm_verify_string(vm_verify_flag_t flag, const char* string, vm_area_t* vma)
{
    return vm_verify_string_impl(flag, string, -1, vma);
}

int vm_verify_string_limit(vm_verify_flag_t flag, const char* string, size_t limit, vm_area_t* vma)
{
    return vm_verify_string_impl(flag, string, limit, vma);
}
