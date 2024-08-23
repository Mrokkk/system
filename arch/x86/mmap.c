#include <arch/mmap.h>
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/list.h>
#include <kernel/page.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/process.h>
#include <kernel/vm_print.h>

#define DEBUG_MMAP 0
#define DEBUG_BRK  0

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
    return (prot & PROT_READ ? VM_READ : 0)
        | (prot & PROT_WRITE ? VM_WRITE : 0)
        | (prot & PROT_EXEC ? VM_EXEC : 0);
}

void* do_mmap(void* addr, size_t len, int prot, int flags, file_t* file, size_t offset)
{
    int errno;
    uint32_t vaddr;
    vm_area_t* vma;
    size_t size = page_align(len);

    log_debug(DEBUG_MMAP, "addr = %x, size = %x, prot = %x, flags = %x, file = %x", addr, size, prot, flags, file);

    if (unlikely(!size || (offset & PAGE_MASK) || (addr(addr) & PAGE_MASK)))
    {
        return ptr(-EINVAL);
    }

    if (flags & MAP_ANONYMOUS && file != NULL)
    {
        return ptr(-EINVAL);
    }

    scoped_mutex_lock(&process_current->mm->lock);

    if (flags & MAP_FIXED)
    {
        if (!addr)
        {
            return ptr(-EINVAL);
        }
        if (unlikely((uintptr_t)addr >= KERNEL_PAGE_OFFSET || (uintptr_t)addr + size > KERNEL_PAGE_OFFSET))
        {
            return ptr(-EFAULT);
        }
        if (!current_vm_verify(VERIFY_READ, addr))
        {
            // Fail if address is already mapped
            return ptr(-EINVAL);
        }
        vaddr = addr(addr);
    }
    else if (unlikely(!(vaddr = address_space_find(size))))
    {
        return ptr(-ENOMEM);
    }

    if (unlikely(!(vma = vm_create(vaddr, size, vm_flags_get(prot)))))
    {
        return ptr(-ENOMEM);
    }

    if (flags & MAP_ANONYMOUS)
    {
        vma->dentry = NULL;
        vma->offset = 0;
    }
    else
    {
        if (unlikely(!file))
        {
            current_log_error("file is NULL");
            return ptr(-EINVAL);
        }

        if (unlikely(!file->ops || !file->ops->mmap))
        {
            log_debug(DEBUG_MMAP, "no operation");
            errno = -ENOSYS;
            goto free_vma;
        }

        log_debug(DEBUG_MMAP, "calling ops->mmap");
        if (unlikely(errno = file->ops->mmap(file, vma)))
        {
            log_debug(DEBUG_MMAP, "ops->mmap failed");
            goto free_vma;
        }

        if (unlikely(inode_get(file->dentry->inode)))
        {
            return ptr(-ENOENT);
        }

        vma->dentry = file->dentry;
        vma->offset = offset;
    }

    if (unlikely(errno = vm_add(&process_current->mm->vm_areas, vma)))
    {
        goto free_vma;
    }

    log_debug(DEBUG_MMAP, "returning %x; vm area:", vaddr);
    vm_area_log_debug(DEBUG_MMAP, vma);

    return ptr(vaddr);

free_vma:
    if (vma->dentry)
    {
        inode_put(vma->dentry->inode);
    }
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
    vm_area_t* vma;

    scoped_mutex_lock(&process_current->mm->lock);

    vma = vm_find(addr(addr), process_current->mm->vm_areas);

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

    return 0;
}

int sys_brk(void* addr)
{
    int errno;

    if (unlikely(!addr))
    {
        return 0;
    }

    scoped_mutex_lock(&process_current->mm->lock);

    vm_area_t* new_brk_vma = vm_find(addr(addr), process_current->mm->vm_areas);
    vm_area_t* old_brk_vma = process_current->mm->brk_vma;

    current_log_debug(DEBUG_BRK, "%x, new vma: %x old: %x", addr, new_brk_vma, old_brk_vma);

    if (unlikely(!old_brk_vma))
    {
        current_log_error("brk vm_area missing; brk address: %x", process_current->mm->brk);
        return -ENOMEM;
    }

    if (old_brk_vma == new_brk_vma)
    {
        process_current->mm->brk = addr(addr);
        return 0;
    }

    if (!new_brk_vma)
    {
        new_brk_vma = vm_create(addr(addr), 0, VM_READ | VM_WRITE | VM_TYPE(VM_TYPE_HEAP));

        if (unlikely(!new_brk_vma))
        {
            current_log_debug(DEBUG_BRK, "cannot allocate brk vma");
            return -ENOMEM;
        }

        if (unlikely(errno = vm_add(&process_current->mm->vm_areas, new_brk_vma)))
        {
            current_log_debug(DEBUG_BRK, "cannot add brk vma to mm");
            return errno;
        }
    }

    process_current->mm->brk_vma = new_brk_vma;
    process_current->mm->brk = addr(addr);

    vm_unmap(old_brk_vma, process_current->mm->pgd);
    vm_del(old_brk_vma);

    return 0;
}

void* sys_sbrk(int incr)
{
    vm_area_t* brk_vma;
    uint32_t previous_brk, current_brk, previous_page, next_page;

    scoped_mutex_lock(&process_current->mm->lock);

    previous_brk = process_current->mm->brk;
    current_brk = previous_brk + incr;
    previous_page = page_beginning(previous_brk);
    next_page = page_align(current_brk);

    current_log_debug(DEBUG_BRK, "incr=%x previous_brk=%x", incr, previous_brk);

    brk_vma = process_current->mm->brk_vma;

    if (unlikely(!brk_vma))
    {
        current_log_error("brk vm_area missing; brk address: %x", process_current->mm->brk);
        process_vm_areas_log(KERN_ERR, process_current);
        return ptr(-ENOMEM);
    }

    if (previous_page > next_page)
    {
        vm_unmap_range(brk_vma, next_page, previous_page, process_current->mm->pgd);
        pgd_reload();
    }

    brk_vma->end = next_page;
    process_current->mm->brk = current_brk;
    vm_area_log_debug(DEBUG_BRK, brk_vma);

    return ptr(previous_brk);
}
