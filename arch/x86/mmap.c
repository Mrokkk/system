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
    uint32_t last_start;
    vm_area_t* vma = process_current->mm->vm_areas;
    for (; vma->next; vma = vma->next);
    last_start = vma->start;
    vma = vma->prev;
    for (; vma->prev; vma = vma->prev)
    {
        if (last_start - vma->end >= size)
        {
            return min(last_start, USER_STACK_VIRT_ADDRESS) - size;
        }
        last_start = vma->start;
    }
    return 0;
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

    if (unlikely(!size || (offset & PAGE_MASK)))
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

    log_debug(DEBUG_MMAP, "addr = %x, size = %x, prot = %x, flags = %x, file = %x", vaddr, size, prot, flags, file);

    vma = vm_create(vaddr, size, vm_flags_get(prot));

    if (unlikely(!vma))
    {
        return ptr(-ENOMEM);
    }

    if (flags & MAP_ANONYMOUS)
    {
        pages = page_alloc(page_count, PAGE_ALLOC_DISCONT);
        list_merge(&vma->pages->head, &pages->list_entry);
        vma->inode = NULL;
    }
    else
    {
        if (!file->ops || !file->ops->mmap)
        {
            log_debug(DEBUG_MMAP, "no operation");
            return ptr(-ENOSYS);
        }

        log_debug(DEBUG_MMAP, "calling ops->mmap");
        if ((errno = file->ops->mmap(file, vma, offset)))
        {
            log_debug(DEBUG_MMAP, "ops->mmap failed");
            return ptr(errno);
        }

        vma->inode = prot & PROT_WRITE ? NULL : file->inode;
    }

    if (prot & PROT_EXEC)
    {
        process_current->mm->code_start = vma->start;
        process_current->mm->code_end = vma->end;
    }

    if (!list_empty(&vma->pages->head))
    {
        page_t* pages = list_front(&vma->pages->head, page_t, list_entry);
        if (unlikely(errno = vm_map(vma, pages, process_current->mm->pgd, 0)))
        {
            goto free_vma;
        }
    }

    mutex_lock(&process_current->mm->lock);

    if (unlikely(errno = vm_add(&process_current->mm->vm_areas, vma)))
    {
        goto remove_vma;
    }

    mutex_unlock(&process_current->mm->lock);

    log_debug(DEBUG_MMAP, "returning %x", vaddr);

    return ptr(vaddr);

remove_vma:
    mutex_unlock(&process_current->mm->lock);
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

void* sys_sbrk(size_t incr)
{
    page_t* new_page;
    vm_area_t* brk_vma;
    uint32_t previous_brk = process_current->mm->brk;
    uint32_t current_brk = previous_brk + incr;
    uint32_t previous_page = page_beginning(previous_brk);
    uint32_t next_page = page_align(current_brk);

    log_debug(DEBUG_MMAP, "incr=%x previous_brk=%x", incr, previous_brk);

    brk_vma = process_brk_vm_area(process_current);

    if (unlikely(!brk_vma))
    {
        vm_print(process_current->mm->vm_areas, DEBUG_MMAP);
        panic("no brk vma; brk = %x", previous_brk, brk_vma);
        ASSERT_NOT_REACHED();
    }
    else if (brk_vma->pages->count > 1)
    {
        // Get own pages
        vm_copy_on_write(brk_vma, process_current->mm->pgd);
    }

    if (previous_page == next_page)
    {
        log_debug(DEBUG_MMAP, "no need to allocate a page; prev=%x; next=%x", previous_brk, current_brk);
        process_current->mm->brk = current_brk;
        return ptr(previous_brk);
    }

    new_page = page_alloc((next_page - previous_page) / PAGE_SIZE, PAGE_ALLOC_DISCONT);

    if (unlikely(!new_page))
    {
        return ptr(-ENOMEM);
    }

    vm_map(brk_vma, new_page, process_current->mm->pgd, VM_APPLY_EXTEND);

    process_current->mm->brk = current_brk;

    vm_print_short(process_current->mm->vm_areas, DEBUG_MMAP);

    return ptr(previous_brk);
}
