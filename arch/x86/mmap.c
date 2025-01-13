#include <stdint.h>
#include <arch/mmap.h>
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/list.h>
#include <kernel/kernel.h>
#include <kernel/minmax.h>
#include <kernel/process.h>
#include <kernel/vm_print.h>
#include <kernel/page_table.h>

#define DEBUG_MMAP 0
#define DEBUG_BRK  0

static inline uintptr_t address_space_find(size_t size)
{
    uintptr_t as_start = 0x1000;
    uintptr_t as_end =
#if CONFIG_SEGMEXEC
        CODE_START - USER_STACK_SIZE;
#else
        USER_STACK_VIRT_ADDRESS - USER_STACK_SIZE;
#endif

    uintptr_t last_start;
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
    size_t file_size = len;
    size_t size = page_align(len);

    current_log_debug(DEBUG_MMAP, "addr = %x, size = %x, prot = %x, flags = %x, file = %x", addr, size, prot, flags, file);

    if (unlikely(!size
        || (offset & PAGE_MASK)
        || (addr(addr) & PAGE_MASK))
        || ((prot & (PROT_EXEC | PROT_WRITE)) == (PROT_EXEC | PROT_WRITE)))
    {
        return ptr(-EINVAL);
    }

    if (unlikely(flags & MAP_ANONYMOUS && file != NULL))
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
            log_debug(DEBUG_MMAP, "ops->mmap failed with %d", errno);
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

    vma->actual_end = vma->start + file_size;
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

static int vma_range_find(uintptr_t addr, size_t len, vm_area_t** vma)
{
    vm_area_t* next;

    for (vm_area_t* temp = process_current->mm->vm_areas; temp; temp = temp->next)
    {
        if (address_within(addr, temp))
        {
            *vma = temp;

            // If we found vma with given vaddr, then check if
            // requested range is valid
            while (addr + len > temp->end)
            {
                // If len extends beyond vma, then check if next
                // exists and adheres to vma
                if (unlikely(!(next = temp->next)))
                {
                    return -ENOMEM;
                }

                if (next->start != temp->end)
                {
                    return -ENOMEM;
                }

                if (unlikely(next->vm_flags & VM_IMMUTABLE))
                {
                    return -EPERM;
                }

                temp = next;
            }

            if (unlikely(temp->vm_flags & VM_IMMUTABLE))
            {
                return -EPERM;
            }

            return 0;
        }
    }

    return -ENOMEM;
}

int sys_munmap(void* addr, size_t len)
{
    int errno;
    vm_area_t* vma;
    vm_area_t* temp;
    uintptr_t start = addr(addr);
    uintptr_t end = start + len;
    uintptr_t old_start;

    if (unlikely(addr(addr) % PAGE_SIZE || len % PAGE_SIZE))
    {
        return -EINVAL;
    }

    if (unlikely(errno = vma_range_find(addr(addr), len, &vma)))
    {
        return errno;
    }

    if (unlikely(!vma))
    {
        return -EINVAL;
    }

    if (vma->start == start && vma->end == end)
    {
        vm_unmap(vma, process_current->mm->pgd);
        vm_del(vma);
        return 0;
    }

    do
    {
        // TODO: remove this restriction
        if (UNLIKELY(start > vma->start && end < vma->end))
        {
            return -EINVAL;
        }

        if (start > vma->start)
        {
            vma->end = start;
            vm_unmap_range(vma, start, vma->end, process_current->mm->pgd);
        }
        else if (vma->end > end)
        {
            old_start = vma->start;
            start = vma->start = end;
            vm_unmap_range(vma, old_start, end, process_current->mm->pgd);
        }
        else
        {
            start = vma->end;
            temp = vma->next;
            vm_unmap(vma, process_current->mm->pgd);
            vm_del(vma);
            vma = temp;
        }
    }
    while (start < end);

    return 0;
}

static bool vmas_can_be_merged(const vm_area_t* vma1, const vm_area_t* vma2)
{
    return vma1->vm_flags == vma2->vm_flags
        && !vma1->dentry && !vma2->dentry
        && (vma1->start == vma2->end || vma1->end == vma2->start);
}

static void vm_copy_details(vm_area_t* to, const vm_area_t* from)
{
    to->offset = from->offset;
    to->dentry = from->dentry;
    if (to->dentry)
    {
        to->offset += to->start - from->start;
    }
    to->ops = from->ops;
}

int sys_mprotect(void* addr, size_t len, int prot)
{
    int errno;
    vm_area_t* vma;
    vm_area_t* new_vma = NULL;
    vm_area_t* new_vmas = NULL;
    vm_area_t* new_vmas_end = NULL;
    vm_area_t* replace_start = NULL;
    vm_area_t* replace_end = NULL;
    uintptr_t start, end;
    int vm_flags = vm_flags_get(prot);

    if (unlikely((addr(addr) & PAGE_MASK) | (len & PAGE_MASK))
        || (prot & (PROT_EXEC | PROT_WRITE)) == (PROT_EXEC | PROT_WRITE))
    {
        return -EINVAL;
    }

    scoped_mutex_lock(&process_current->mm->lock);

    if (unlikely(errno = vma_range_find(addr(addr), len, &vma)))
    {
        return errno;
    }

    if (unlikely(!vma))
    {
        return -ENOMEM;
    }

    start = addr(addr);
    end = start + len;

    if (vma->start == start && vma->end == end)
    {
        new_vmas = vma;
        vma->vm_flags = vm_flags;
        goto apply;
    }

    replace_start = vma;

    do
    {
        replace_end = vma;
        vm_area_t* prev_vma = vma->prev;
        vm_area_t* next_vma = vma->next;

        if (new_vma && !new_vma->dentry && !vma->dentry)
        {
            if (vma->vm_flags == new_vma->vm_flags)
            {
                start = new_vma->end = vma->end;
            }
            else
            {
                start = new_vma->end = min(vma->end, end);
            }
        }

        #define safe_vm_create(...) \
            ({ \
                vm_area_t* v = vm_create(__VA_ARGS__); \
                if (unlikely(!v)) \
                { \
                    errno = -ENOMEM; \
                    goto error; \
                } \
                v; \
            })

        if (start < vma->end)
        {
            if (start > vma->start)
            {
                new_vma = safe_vm_create(vma->start, start - vma->start, vma->vm_flags);
                vm_copy_details(new_vma, vma);
                vm_add(&new_vmas, new_vma);
            }

            new_vma = safe_vm_create(start, min(vma->end - start, end - start), vm_flags);
            vm_copy_details(new_vma, vma);

            if (prev_vma && vmas_can_be_merged(prev_vma, new_vma))
            {
                new_vma->start = prev_vma->start;
                replace_start = prev_vma;
            }
            if (next_vma && vmas_can_be_merged(next_vma, new_vma))
            {
                new_vma->end = next_vma->end;
                replace_end = next_vma;
            }

            vm_add(&new_vmas, new_vma);

            if (end < vma->end)
            {
                new_vma = vm_create(end, vma->end - end, vma->vm_flags);
                vm_copy_details(new_vma, vma);
                vm_add(&new_vmas, new_vma);
            }
        }

        new_vmas_end = new_vma;
        start = new_vma->end;
        vma = vma->next;

        if (!vma)
        {
            break;
        }
    }
    while (start < end);

    if (unlikely(start < end))
    {
        errno = -ENOMEM;
        goto error;
    }

    vm_replace(&process_current->mm->vm_areas, new_vmas, new_vmas_end, replace_start, replace_end);

apply:
    if (unlikely(vm_apply(new_vmas, process_current->mm->pgd, addr(addr), addr(addr) + len)))
    {
        siginfo_t siginfo = {
            .si_code = SI_TIMER,
            .si_pid = process_current->pid,
            .si_signo = SIGBUS,
        };

        do_kill(process_current, &siginfo);
        return -ENOMEM;
    }

    tlb_flush();

    return 0;

error:
    if (new_vmas)
    {
        vm_areas_del(new_vmas);
    }

    return errno;
}

int sys_mimmutable(void* addr, size_t len)
{
    vm_area_t* vma;
    uintptr_t start = addr(addr);
    const uintptr_t end = start + len;

    // TODO: this require some changes to properly validate requested
    // range and to be able to divide vmas

    if (unlikely((addr(addr) & PAGE_MASK) | (len & PAGE_MASK)))
    {
        return -EINVAL;
    }

    scoped_mutex_lock(&process_current->mm->lock);

    if (unlikely(!(vma = vm_find(addr(addr), process_current->mm->vm_areas))))
    {
        return -EINVAL;
    }

    while (start < end)
    {
        vma->vm_flags |= VM_IMMUTABLE;
        vma = vma->next;
        start = vma->end;

        if (!vma)
        {
            break;
        }

        start = vma->start;
    }

    if (unlikely(start < end))
    {
        return -EINVAL;
    }

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
        current_log_error("brk vm_area missing; brk address: %x", previous_brk);
        process_vm_areas_log(KERN_ERR, process_current);
        return ptr(-ENOMEM);
    }

    if (incr > 0)
    {
        if (unlikely(next_page < previous_page))
        {
            return ptr(-ENOMEM);
        }
        if (unlikely(brk_vma->next && brk_vma->next->start <= next_page))
        {
            return ptr(-ENOMEM);
        }
    }
    else
    {
        if (unlikely(next_page > previous_page))
        {
            return ptr(-ENOMEM);
        }
        if (unlikely(brk_vma->prev && brk_vma->prev->end > next_page))
        {
            return ptr(-ENOMEM);
        }
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

int sys_pinsyscalls(void* start, size_t size)
{
    vm_area_t* vma;
    process_t* p = process_current;

    scoped_mutex_lock(&p->mm->lock);

    if (unlikely(p->mm->syscalls_start))
    {
        return -EPERM;
    }

    vma = vm_find(addr(start), p->mm->vm_areas);

    if (unlikely(!vma || (vma->end < addr(start) + size) || !(vma->vm_flags & VM_EXEC)))
    {
        return -EINVAL;
    }

    p->mm->syscalls_start = addr(start);
    p->mm->syscalls_end = addr(start) + size;

    return 0;
}
