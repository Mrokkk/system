#include <arch/vm.h>
#include <arch/mmap.h>
#include <kernel/fs.h>
#include <kernel/page.h>
#include <kernel/process.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>

#define DEBUG_MMAP 1

struct mmap_params
{
    void* addr;
    size_t len;
    int prot;
    int flags;
    int fd;
    size_t off;
};

static inline uint32_t address_space_find(size_t size)
{
    uint32_t last_start;
    vm_area_t* vma = process_current->mm->vm_areas;
    for (; vma->next; vma = vma->next);
    last_start = vma->virt_address;
    vma = vma->prev;
    for (; vma->prev; vma = vma->prev)
    {
        if (last_start - vma->virt_address >= size)
        {
            return min(last_start, USER_SIGRET_VIRT_ADDRESS) - size;
        }
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

void* do_mmap(void* addr, size_t len, int prot, int flags, int fd, size_t offset)
{
    int errno;
    file_t* file;
    page_t* pages;
    uint32_t vaddr;
    vm_area_t* vma;
    size_t size = page_align(len);
    size_t page_count = size / PAGE_SIZE;

    if (unlikely(!size || (offset & PAGE_MASK)))
    {
        return ptr(-EINVAL);
    }

    if (prot & PROT_WRITE && !(flags & MAP_ANONYMOUS))
    {
        // TODO: add support for writable mapped files
        return ptr(-ENOSYS);
    }

    if (flags & MAP_ANONYMOUS && fd != -1)
    {
        return ptr(-EINVAL);
    }

    if (flags & MAP_FIXED)
    {
        vaddr = addr(addr);
    }
    else
    {
        vaddr = address_space_find(size);
    }

    log_debug(DEBUG_MMAP, "addr = %x, size = %x, prot = %x, flags = %x, fd = %d", vaddr, size, prot, flags, fd);

    vma = alloc(vm_area_t);

    if (unlikely(!vma))
    {
        return ptr(-ENOMEM);
    }

    vma->pages = alloc(pages_t);

    if (unlikely(!vma->pages))
    {
        return ptr(-ENOMEM);
    }

    vma->pages->count = 1;
    list_init(&vma->pages->head);
    vma->virt_address = vaddr;
    vma->size = size;
    vma->vm_flags = vm_flags_get(prot);
    vma->next = NULL;
    vma->prev = NULL;

    if (flags & MAP_ANONYMOUS)
    {
        pages = page_alloc(page_count, PAGE_ALLOC_DISCONT);
        list_merge(&vma->pages->head, &pages->list_entry);
    }
    else
    {
        if (fd_check_bounds(fd)) return ptr(-EBADF);
        if (process_fd_get(process_current, fd, &file)) return ptr(-EBADF);

        if (!file->ops || !file->ops->mmap2)
        {
            log_debug(DEBUG_MMAP, "no operation");
            return ptr(-ENOSYS);
        }

        log_debug(DEBUG_MMAP, "calling ops->mmap");
        if ((errno = file->ops->mmap2(file, vma, offset)))
        {
            log_debug(DEBUG_MMAP, "ops->mmap failed");
            return ptr(errno);
        }
    }

    mutex_lock(&process_current->mm->lock);

    if (unlikely(errno = vm_add(&process_current->mm->vm_areas, vma)))
    {
        goto free_vma;
    }

    if (unlikely(errno = vm_apply(vma, process_current->mm->pgd, VM_APPLY_REPLACE_PG)))
    {
        goto remove_vma;
    }

    mutex_unlock(&process_current->mm->lock);

    log_debug(DEBUG_MMAP, "returning %x", vaddr);

    return ptr(vaddr);

remove_vma:
free_vma:
    mutex_unlock(&process_current->mm->lock);
    return ptr(errno);
}

void* sys_mmap(struct mmap_params* params)
{
    return do_mmap(params->addr, params->len, params->prot, params->flags, params->fd, params->off);
}

void* sys_sbrk(size_t incr)
{
    vm_area_t* brk_vma;
    uint32_t previous_brk = process_current->mm->brk;
    uint32_t current_brk = previous_brk + incr;
    uint32_t previous_page = page_beginning(previous_brk);
    uint32_t next_page = page_align(current_brk);

    log_debug(DEBUG_PROCESS, "incr=%x previous_brk=%x", incr, previous_brk);

    brk_vma = process_brk_vm_area(process_current);

    if (unlikely(!brk_vma))
    {
        vm_print(process_current->mm->vm_areas, DEBUG_PROCESS);
        panic("no brk vma; brk = %x", previous_brk, brk_vma);
        ASSERT_NOT_REACHED();
    }
    else if (brk_vma->pages->count > 1)
    {
        // Get own pages
        vm_copy_on_write(brk_vma);
    }

    if (previous_page == next_page)
    {
        log_debug(DEBUG_PROCESS, "no need to allocate a page; prev=%x; next=%x", previous_brk, current_brk);
        process_current->mm->brk = current_brk;
        return ptr(previous_brk);
    }

    uint32_t needed_pages = (next_page - previous_page) / PAGE_SIZE;

    page_t* new_page = page_alloc(needed_pages, PAGE_ALLOC_DISCONT);

    if (unlikely(!new_page))
    {
        return ptr(-ENOMEM);
    }

    vm_extend(
        brk_vma,
        new_page,
        0,
        needed_pages);

    vm_apply(
        brk_vma,
        ptr(process_current->mm->pgd),
        VM_APPLY_DONT_REPLACE);

    process_current->mm->brk = current_brk;

    return ptr(previous_brk);
}
