#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/vm_print.h>

#define COW_BROKEN 1

vm_area_t* vm_create(uint32_t vaddr, uint32_t size, int vm_flags)
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

    vma->pages->refcount = 1;
    list_init(&vma->pages->head);
    vma->start = vaddr;
    vma->end = vaddr + size;
    vma->vm_flags = vm_flags;
    vma->inode = NULL;
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
}

int vm_copy(vm_area_t* dest_vma, const vm_area_t* src_vma, pgd_t* dest_pgd, const pgd_t* src_pgd)
{
    // Copy vm_area except for next and prev
    memcpy(dest_vma, src_vma, sizeof(vm_area_t) - 2 * sizeof(struct vm_area*));

    dest_vma->next = NULL;
    dest_vma->prev = NULL;
    dest_vma->pages->refcount++;
    dest_vma->vm_flags |= VM_COW;

    // FIXME: random bash crash was caused by COW implemented poorly -
    // I wasn't  denying write access on parent pages, so parent could
    // overwrite child's memory
#if !COW_BROKEN
    return arch_vm_copy(dest_pgd, src_pgd, src_vma->start, src_vma->end);
#else
    int errno;
    if ((errno = arch_vm_copy(dest_pgd, src_pgd, src_vma->start, src_vma->end)))
    {
        return errno;
    }

    return dest_vma->vm_flags & VM_WRITE
        ? vm_copy_on_write(dest_vma, dest_pgd)
        : 0;
#endif
}

int vm_copy_on_write(vm_area_t* vma, const pgd_t* pgd)
{
    page_t* page_range;

    vm_area_log_debug(DEBUG_VM_COW, vma);

    if (unlikely(!--vma->pages->refcount))
    {
        log_error("cow on not shared pages!");
        return -EFAULT;
    }

    page_range = page_alloc((vma->end - vma->start) / PAGE_SIZE, PAGE_ALLOC_CONT);

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

    vma->pages->refcount = 1;
    list_init(&vma->pages->head);
    list_merge(&vma->pages->head, &page_range->list_entry);
    vma->vm_flags &= ~VM_COW;

    return arch_vm_copy_on_write(vma, pgd, page_range);
}

int vm_map(vm_area_t* vma, page_t* new_pages, pgd_t* pgd, int vm_apply_flags)
{
    int errno;
    uint32_t start, end;

    start = vm_apply_flags & VM_APPLY_EXTEND ? vma->end : vma->start;
    end = start + new_pages->pages_count * PAGE_SIZE;

    if (unlikely(errno = arch_vm_apply(pgd, new_pages, start, end, vma->vm_flags)))
    {
        log_debug(DEBUG_VM_APPLY, "failed %d", errno);
        return errno;
    }

    vma->end = end;

    pgd_reload();

    return errno;
}

int vm_remap(vm_area_t* vma, page_t* pages, pgd_t* pgd)
{
    int errno;
    uint32_t start, end;

    start = vma->start;
    end = start + pages->pages_count * PAGE_SIZE;

    if (unlikely(errno = arch_vm_reapply(pgd, pages, start, end, vma->vm_flags)))
    {
        log_debug(DEBUG_VM_APPLY, "failed %d", errno);
        return errno;
    }

    vma->end = end;

    pgd_reload();

    return errno;
}

static inline int address_within(uint32_t vaddr, vm_area_t* vma)
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

int __vm_verify(vm_verify_flag_t verify, uint32_t vaddr, size_t size, vm_area_t* vma)
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

int vm_verify_string(vm_verify_flag_t flag, const char* string, vm_area_t* vma)
{
    // FIXME: hack for kernel processes
    if (unlikely(!vma))
    {
        return 0;
    }

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
