#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/minmax.h>
#include <kernel/signal.h>
#include <kernel/process.h>
#include <kernel/segmexec.h>
#include <kernel/vm_print.h>

#define DEBUG_NOPAGE 0

vm_area_t* vm_create(uintptr_t vaddr, size_t size, int vm_flags)
{
    vm_area_t* vma = alloc(vm_area_t);

    if (unlikely(!vma))
    {
        log_debug(DEBUG_VM, "cannot allocate vm_area");
        return NULL;
    }

    vma->start = vma->end = vaddr;
    vma->actual_end = vma->end += size;
    vma->vm_flags = vm_flags;
    vma->dentry = NULL;
    vma->next = NULL;
    vma->prev = NULL;
    vma->offset = 0;

    return vma;
}

int vm_add(vm_area_t** head, vm_area_t* new_vma)
{
    uintptr_t new_end = new_vma->end;
    uintptr_t new_start = new_vma->start;

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

void vm_add_tail(vm_area_t* new_vma, vm_area_t* old_vma)
{
    new_vma->prev = old_vma;
    new_vma->next = old_vma->next;
    if (old_vma->next)
    {
        old_vma->next->prev = new_vma;
    }
    old_vma->next = new_vma;
}

void vm_add_front(vm_area_t* new_vma, vm_area_t* old_vma)
{
    new_vma->next = old_vma;
    new_vma->prev = old_vma->prev;
    if (old_vma->prev)
    {
        old_vma->prev->next = new_vma;
    }
    old_vma->prev = new_vma;
}

void vm_del(vm_area_t* vma)
{
    if (vma->next)
    {
        vma->next->prev = vma->prev;
    }
    if (vma->prev)
    {
        vma->prev->next = vma->next;
    }
    delete(vma);
}

void vm_areas_del(vm_area_t* vmas)
{
    vm_area_t* temp;
    for (vm_area_t* v = vmas; v;)
    {
        temp = v->next;
        delete(v);
        v = temp;
    }
}

void vm_replace(
    vm_area_t** vm_areas,
    vm_area_t* new_vmas,
    vm_area_t* new_vmas_end,
    vm_area_t* replace_start,
    vm_area_t* replace_end)
{
    if (replace_start == *vm_areas)
    {
        *vm_areas = new_vmas;
    }
    else
    {
        replace_start->prev->next = new_vmas;
        new_vmas->prev = replace_start->prev;
    }

    new_vmas_end->next = replace_end->next;

    if (likely(replace_end->prev))
    {
        replace_end->next->prev = new_vmas_end;
    }

    replace_end->next = NULL;

    vm_areas_del(replace_start);
}

int vm_copy(vm_area_t* dest_vma, const vm_area_t* src_vma, pgd_t* dest_pgd, pgd_t* src_pgd)
{
    // Copy vm_area except for next and prev
    memcpy(dest_vma, src_vma, sizeof(vm_area_t) - 2 * sizeof(vm_area_t*));

    dest_vma->next = NULL;
    dest_vma->prev = NULL;

    return arch_vm_copy(dest_vma, dest_pgd, src_pgd, src_vma->start, src_vma->end);
}

int vm_nopage(vm_area_t* vma, pgd_t* pgd, uintptr_t address, bool write)
{
    int errno, res;
    uintptr_t pde_index, pte_index;
    size_t size;

    if (unlikely(!(vma->vm_flags & VM_READ)))
    {
        return -EFAULT;
    }

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

        size = vma->actual_end - page_beginning(address);
        size = min(size, PAGE_SIZE);

        res = vma->ops->nopage(vma, address, size, &page);

        if (unlikely(errno = errno_get(res)))
        {
            log_warning("ops->nopage failed: %d", errno);
            return errno;
        }

        if (res != PAGE_SIZE)
        {
            memset(page_virt_ptr(page) + res, 0, PAGE_SIZE - res);
        }

        goto map_page;
    }

    if (!page)
    {
        page = page_alloc1();

        if (unlikely(!page))
        {
            current_log_info("OOM on %x", address);
            do_kill(process_current, SIGKILL);
            return 0;
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

#if CONFIG_SEGMEXEC
    if (vma->vm_flags & VM_EXEC)
    {
        pde_index += CODE_START / (PAGE_SIZE * PAGES_IN_PTE);
        if (unlikely(errno = arch_vm_map_single(pgd, pde_index, pte_index, page, vma->vm_flags)))
        {
            log_warning("arch_vm_map_single failed with %d", errno);
            return errno;
        }
    }
#endif

    pgd_reload();

    return 0;
}

vm_area_t* vm_find(uintptr_t vaddr, vm_area_t* vmas)
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

int vm_verify_impl(vm_verify_flag_t verify, uintptr_t vaddr, size_t size, vm_area_t* vma)
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
