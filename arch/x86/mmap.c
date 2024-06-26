#include <arch/mmap.h>
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/process.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>

#define DEBUG_MMAP 0

static inline uint32_t address_space_find(size_t size)
{
    uint32_t as_start = 0x1000;
    uint32_t as_end = USER_STACK_VIRT_ADDRESS - USER_STACK_SIZE;

    uint32_t last_start;
    vm_area_t* vma = process_current->mm->vm_areas;

    if (!vma)
    {
        goto finish;
    }

    for (; vma->next; vma = vma->next);

    last_start = vma->start;
    vma = vma->prev;

    if (!vma)
    {
        return last_start - size;
    }

    for (; vma->prev; vma = vma->prev)
    {
        if (last_start - vma->end >= size)
        {
            return min(last_start, USER_STACK_VIRT_ADDRESS) - size;
        }
        last_start = vma->start;
    }

    if (unlikely(last_start - as_start < size))
    {
        return 0;
    }

finish:
    return as_end - size;
}

static inline int vm_flags_get(int prot)
{
    int vm_flags = prot & PROT_READ ? VM_READ : 0;
    vm_flags |= prot & PROT_WRITE ? VM_WRITE : 0;
    vm_flags |= prot & PROT_EXEC ? VM_EXEC : 0;
    return vm_flags;
}

void* do_mmap(void* addr, size_t len, int prot, int flags, file_t* file, size_t offset)
{
    int errno;
    page_t* pages;
    uint32_t vaddr;
    vm_area_t* vma;
    size_t size = page_align(len);
    size_t page_count = size / PAGE_SIZE;

    log_debug(DEBUG_MMAP, "addr = %x, size = %x, prot = %x, flags = %x, file = %x", addr, size, prot, flags, file);

    if (unlikely(!size || (offset & PAGE_MASK) || (addr(addr) & PAGE_MASK)))
    {
        return ptr(-EINVAL);
    }

    if (flags & MAP_ANONYMOUS && file != NULL)
    {
        return ptr(-EINVAL);
    }

    if (flags & MAP_FIXED)
    {
        if (!addr)
        {
            return ptr(-EINVAL);
        }
        if (!current_vm_verify(VERIFY_READ, addr))
        {
            // Fail if address is already mapped
            return ptr(-EINVAL);
        }
        vaddr = addr(addr);
    }
    else
    {
        vaddr = address_space_find(size);
        if (!vaddr)
        {
            return ptr(-ENOMEM);
        }
    }

    vma = vm_create(vaddr, size, vm_flags_get(prot));

    if (unlikely(!vma))
    {
        return ptr(-ENOMEM);
    }

    pages = page_alloc(page_count, PAGE_ALLOC_DISCONT);

    if (unlikely(!pages))
    {
        errno = -ENOMEM;
        goto free_vma;
    }

    list_merge(&vma->pages->head, &pages->list_entry);

    if (flags & MAP_ANONYMOUS)
    {
        vma->inode = NULL;
    }
    else
    {
        if (!file->ops || !file->ops->mmap)
        {
            log_debug(DEBUG_MMAP, "no operation");
            errno = -ENOSYS;
            goto free_pages;
        }

        log_debug(DEBUG_MMAP, "calling ops->mmap");
        if ((errno = file->ops->mmap(file, vma, pages, page_count * PAGE_SIZE, offset)))
        {
            log_debug(DEBUG_MMAP, "ops->mmap failed");
            goto free_pages;
        }

        vma->inode = prot & PROT_WRITE ? NULL : file->inode;
    }

    if (prot & PROT_EXEC)
    {
        process_current->mm->code_start = vma->start;
        process_current->mm->code_end = vma->end;
    }

    mutex_lock(&process_current->mm->lock);

    if (unlikely(errno = vm_map(vma, pages, process_current->mm->pgd, 0)))
    {
        goto unlock_mm;
    }

    if (unlikely(errno = vm_add(&process_current->mm->vm_areas, vma)))
    {
        goto unlock_mm;
    }

    mutex_unlock(&process_current->mm->lock);

    log_debug(DEBUG_MMAP, "returning %x", vaddr);

    return ptr(vaddr);

unlock_mm:
    mutex_unlock(&process_current->mm->lock);
free_pages:
    list_del(&vma->pages->head);
    pages_free(pages);
free_vma:
    delete(vma);
    return ptr(errno);
}

struct mmap_params
{
    void* addr;
    size_t len;
    int prot;
    int flags;
    int fd;
    size_t off;
};

void* sys_mmap(struct mmap_params* params)
{
    file_t* file = NULL;
    if (!(params->flags & MAP_ANONYMOUS))
    {
        if (fd_check_bounds(params->fd)) return ptr(-EBADF);
        if (process_fd_get(process_current, params->fd, &file)) return ptr(-EBADF);
    }
    return do_mmap(params->addr, params->len, params->prot, params->flags, file, params->off);
}

int sys_mprotect(void* addr, size_t len, int prot)
{
    UNUSED(addr); UNUSED(len); UNUSED(prot);
    int errno;
    vm_area_t* vma = vm_find(addr(addr), process_current->mm->vm_areas);

    if (unlikely(!vma))
    {
        return -ENOMEM;
    }

    if (vma->end - vma->start != len)
    {
        // TODO: divide vma
        return -EINVAL;
    }

    vma->vm_flags = vm_flags_get(prot);

    if (unlikely(errno = vm_remap(vma, list_next_entry(&vma->pages->head, page_t, list_entry), process_current->mm->pgd)))
    {
        return errno;
    }

    return 0;
}

int sys_brk(void* addr)
{
    vm_area_t* new_brk_vma = vm_find(addr(addr), process_current->mm->vm_areas);

    if (unlikely(!new_brk_vma))
    {
        return -ENOMEM;
    }

    // TODO: free pages
    process_current->mm->brk = addr(addr);
    return 0;
}

void* sys_sbrk(size_t incr)
{
    int errno;
    page_t* new_page;
    vm_area_t* brk_vma;
    uint32_t previous_brk = process_current->mm->brk;
    uint32_t current_brk = previous_brk + incr;
    uint32_t previous_page = page_beginning(previous_brk);
    uint32_t next_page = page_align(current_brk);

    log_debug(DEBUG_MMAP, "incr=%x previous_brk=%x", incr, previous_brk);

    mutex_lock(&process_current->mm->lock);

    brk_vma = process_brk_vm_area(process_current);

    if (unlikely(!brk_vma))
    {
        vm_print(process_current->mm->vm_areas, DEBUG_MMAP);
        panic("no brk vma; brk = %x", previous_brk, brk_vma);
    }
    else if (brk_vma->pages->refcount > 1)
    {
        // Get own pages
        vm_copy_on_write(brk_vma, process_current->mm->pgd);
    }

    if (previous_page == next_page)
    {
        log_debug(DEBUG_MMAP, "no need to allocate a page; prev=%x; next=%x", previous_brk, current_brk);
        process_current->mm->brk = current_brk;
        mutex_unlock(&process_current->mm->lock);
        return ptr(previous_brk);
    }

    new_page = page_alloc((next_page - previous_page) / PAGE_SIZE, PAGE_ALLOC_NO_KERNEL);

    if (unlikely(!new_page))
    {
        mutex_unlock(&process_current->mm->lock);
        return ptr(-ENOMEM);
    }

    list_merge(&brk_vma->pages->head, &new_page->list_entry);
    if (unlikely(errno = vm_map(brk_vma, new_page, process_current->mm->pgd, VM_APPLY_EXTEND)))
    {
        // FIXME: I should do a cleanup of pages merged to vma
        return ptr(errno);
    }

    process_current->mm->brk = current_brk;

    vm_print_short(process_current->mm->vm_areas, DEBUG_MMAP);

    mutex_unlock(&process_current->mm->lock);

    return ptr(previous_brk);
}
