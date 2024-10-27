#include <kernel/vm.h>
#include <kernel/minmax.h>
#include <kernel/signal.h>
#include <kernel/process.h>
#include <kernel/segmexec.h>
#include <kernel/vm_print.h>
#include <kernel/page_alloc.h>
#include <kernel/page_table.h>

#include <arch/vm.h>

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

static int pte_copy(pud_t* dest_pmd, pud_t* src_pmd, uintptr_t start, uintptr_t end)
{
    uintptr_t next;
    pmd_t* src_pte = pte_offset(src_pmd, start);
    pmd_t* dest_pte = pte_alloc(dest_pmd, start);

    if (unlikely(!dest_pte))
    {
        return -ENOMEM;
    }

    for (uintptr_t vaddr = start; vaddr < end; vaddr = next, src_pte++, dest_pte++)
    {
        next = pte_addr_next(vaddr, end);

        if (pte_entry_none(src_pte))
        {
            continue;
        }

        if (
            1
#if CONFIG_SEGMEXEC
            && start < CODE_START
#endif
            )
        {
            uintptr_t paddr = pte_entry_paddr(src_pte);

            pte_entry_cow(dest_pte, src_pte);
            page(paddr)->refcount++;
        }
    }

    return 0;
}

static int pmd_copy(pud_t* dest_pud, pud_t* src_pud, uintptr_t start, uintptr_t end)
{
    int errno;
    uintptr_t next;

    pmd_t* src_pmd = pmd_offset(src_pud, start);
    pmd_t* dest_pmd = pmd_alloc(dest_pud, start);

    if (unlikely(!dest_pmd))
    {
        return -ENOMEM;
    }

    for (uintptr_t vaddr = start; vaddr != end; vaddr = next, src_pmd++, dest_pmd++)
    {
        next = pmd_addr_next(vaddr, end);

        if (pmd_entry_none(src_pmd))
        {
            continue;
        }

        if (unlikely(errno = pte_copy(dest_pmd, src_pmd, vaddr, next)))
        {
            return errno;
        }
    }

    return 0;
}

static int pud_copy(pgd_t* dest_pgd, pgd_t* src_pgd, uintptr_t start, uintptr_t end)
{
    int errno;
    uintptr_t next;

    pud_t* src_pud = pud_offset(src_pgd, start);
    pud_t* dest_pud = pud_alloc(dest_pgd, start);

    if (unlikely(!dest_pud))
    {
        return -ENOMEM;
    }

    for (uintptr_t vaddr = start; vaddr != end; vaddr = next, src_pud++, dest_pud++)
    {
        next = pud_addr_next(vaddr, end);

        if (pud_entry_none(src_pud))
        {
            continue;
        }

        if (unlikely(errno = pmd_copy(dest_pud, src_pud, vaddr, next)))
        {
            return errno;
        }
    }

    return 0;
}

static int pgd_copy(pgd_t* dest_pgd, pgd_t* src_pgd, uintptr_t start, uintptr_t end)
{
    int errno;
    uintptr_t next;

    src_pgd = pgd_offset(src_pgd, start);
    dest_pgd = pgd_offset(dest_pgd, start);

    for (uintptr_t vaddr = start; vaddr != end; vaddr = next, src_pgd++, dest_pgd++)
    {
        next = pgd_addr_next(vaddr, end);

        if (pgd_entry_none(src_pgd))
        {
            continue;
        }

        if (unlikely(errno = pud_copy(dest_pgd, src_pgd, vaddr, next)))
        {
            return errno;
        }
    }

    return 0;
}

static int vm_copy_impl(vm_area_t* dest_vma, pgd_t* dest_pgd, pgd_t* src_pgd, uintptr_t start, uintptr_t end)
{
    int errno = pgd_copy(dest_pgd, src_pgd, start, end);
    UNUSED(dest_vma);

#if CONFIG_SEGMEXEC
    if (unlikely(errno))
    {
        return errno;
    }

    if (dest_vma->vm_flags & VM_EXEC)
    {
        errno = pgd_copy(dest_pgd, src_pgd, start + CODE_START, end + CODE_START);
    }
#endif

    return errno;
}

int vm_copy(vm_area_t* dest_vma, const vm_area_t* src_vma, pgd_t* dest_pgd, pgd_t* src_pgd)
{
    // Copy vm_area except for next and prev
    memcpy(dest_vma, src_vma, sizeof(vm_area_t) - 2 * sizeof(vm_area_t*));

    dest_vma->next = NULL;
    dest_vma->prev = NULL;

    return vm_copy_impl(dest_vma, dest_pgd, src_pgd, src_vma->start, src_vma->end);
}

static page_t* vm_page(const pgd_t* pgd, const uintptr_t vaddr)
{
    const pgd_t* pgde = pgd_offset(pgd, vaddr);

    if (unlikely(pgd_entry_none(pgde)))
    {
        return NULL;
    }

    const pud_t* pude = pud_offset(pgde, vaddr);

    if (unlikely(pud_entry_none(pude)))
    {
        return NULL;
    }

    const pmd_t* pmde = pmd_offset(pude, vaddr);

    if (unlikely(pmd_entry_none(pmde)))
    {
        return NULL;
    }

    const pte_t* pte = pte_offset(pmde, vaddr);

    if (unlikely(pte_entry_none(pte)))
    {
        return NULL;
    }

    return page(pte_entry_paddr(pte));
}

static int vm_page_map(vm_area_t* vma, pgd_t* pgd, const page_t* page, uintptr_t address)
{
    pgd_t* pgde = pgd_offset(pgd, address);
    pud_t* pude = pud_alloc(pgde, address);

    if (unlikely(!pude))
    {
        return -ENOMEM;
    }

    pmd_t* pmde = pmd_alloc(pude, address);

    if (unlikely(!pmde))
    {
        return -ENOMEM;
    }

    pte_t* pte = pte_alloc(pmde, address);

    if (unlikely(!pte))
    {
        return -ENOMEM;
    }

    pte_entry_set(pte, page_phys(page), vm_to_pgprot(vma));

    return 0;
}

int vm_nopage(pgd_t* pgd, uintptr_t address, bool write)
{
    int errno, res;
    size_t size;
    vm_area_t* vma = vm_find(address, process_current->mm->vm_areas);

    if (unlikely(!vma))
    {
        return -EFAULT;
    }

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

    page_t* page = vm_page(pgd, address);

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

            page_kernel_unmap(new_page);

            page->refcount--;
            page = new_page;
        }
        else if (!write)
        {
            return -EFAULT;
        }
    }

map_page:
    page_kernel_unmap(page);

    if ((errno = vm_page_map(vma, pgd, page, address)))
    {
        return errno;
    }

#if CONFIG_SEGMEXEC
    if (vma->vm_flags & VM_EXEC && (errno = vm_page_map(vma, pgd, page, address + CODE_START)))
    {
        return errno;
    }
#endif

    // TODO: is it needed to flush whole TLB?
    tlb_flush();

    return 0;
}

int vm_apply(vm_area_t* vmas, pgd_t* pgd, uintptr_t vaddr_start, uintptr_t vaddr_end)
{
    const vm_area_t* vma = vmas;
    uintptr_t vaddr;
    pgprot_t prot = vm_to_pgprot(vma);

    for (vaddr = vaddr_start; vaddr < vaddr_end; vaddr += PAGE_SIZE)
    {
        while (vaddr >= vma->end)
        {
            vma = vma->next;

            if (unlikely(!vma))
            {
                log_error("%s:%u:%s: cannot apply vm; vma->next is null", __FILE__, __LINE__, __func__);
                vm_area_log(KERN_ERR, vmas);
                return -1;
            }

            prot = vm_to_pgprot(vma);
        }

        pgd_t* pgde = pgd_offset(pgd, vaddr);

        if (unlikely(pgd_entry_none(pgde)))
        {
            continue;
        }

        pud_t* pude = pud_offset(pgde, vaddr);

        if (unlikely(pud_entry_none(pude)))
        {
            continue;
        }

        pmd_t* pmde = pmd_offset(pude, vaddr);

        if (unlikely(pmd_entry_none(pmde)))
        {
            continue;
        }

        pte_t* pte = pte_offset(pmde, vaddr);

        if (unlikely(pte_entry_none(pte)))
        {
            continue;
        }

        pte_entry_prot_set(pte, prot);
    }

    return 0;
}

uintptr_t vm_paddr(uintptr_t vaddr, const pgd_t* pgd)
{
    const pgd_t* pgde = pgd_offset(pgd, vaddr);

    if (unlikely(pgd_entry_none(pgde)))
    {
        return 0;
    }

    const pud_t* pude = pud_offset(pgde, vaddr);

    if (unlikely(pud_entry_none(pude)))
    {
        return 0;
    }

    const pmd_t* pmde = pmd_offset(pude, vaddr);

    if (unlikely(pmd_entry_none(pmde)))
    {
        return 0;
    }

    const pte_t* pte = pte_offset(pmde, vaddr);

    return pte_entry_paddr(pte);
}

static void pte_range_free(pmd_t* pmd, uintptr_t start, uintptr_t end, uintptr_t floor, uintptr_t ceil, bool free_pages)
{
    uintptr_t next, index;
    pte_t* pte = pte_offset(pmd, start);

    for (uintptr_t vaddr = start; vaddr != end; vaddr = next, pte++)
    {
        next = pte_addr_next(vaddr, end);

        if (pte_entry_none(pte))
        {
            continue;
        }

        if (free_pages)
        {
            pages_free(pte_entry_page(pte));
        }

        pte_entry_clear(pte);
        tlb_flush_single(vaddr);
    }

    index = pmd_index(start);

    if (floor && index == pmd_index(floor))
    {
        return;
    }
    if (ceil && index == pmd_index(ceil))
    {
        return;
    }

    pte_free(pmd);
    pmd_entry_clear(pmd);
}

static void pmd_range_free(pud_t* pud, uintptr_t start, uintptr_t end, uintptr_t floor, uintptr_t ceil, bool free_pages)
{
    uintptr_t next, index;
    pmd_t* pmd = pmd_offset(pud, start);

    for (uintptr_t vaddr = start; vaddr != end; vaddr = next, pmd++)
    {
        next = pmd_addr_next(vaddr, end);

        if (pmd_entry_none(pmd))
        {
            continue;
        }

        pte_range_free(pmd, vaddr, next, floor, ceil, free_pages);
    }

    index = pud_index(start);

    if (floor && index == pud_index(floor))
    {
        return;
    }
    if (ceil && index == pud_index(ceil))
    {
        return;
    }

    pmd_free(pud);
    pud_entry_clear(pud);
}

static void pud_range_free(pgd_t* pgd, uintptr_t start, uintptr_t end, uintptr_t floor, uintptr_t ceil, bool free_pages)
{
    uintptr_t next, index;
    pud_t* pud = pud_offset(pgd, start);

    for (uintptr_t vaddr = start; vaddr != end; vaddr = next, pud++)
    {
        next = pud_addr_next(vaddr, end);

        if (pud_entry_none(pud))
        {
            continue;
        }

        pmd_range_free(pud, vaddr, next, floor, ceil, free_pages);
    }

    index = pgd_index(start);

    if (floor && index == pgd_index(floor))
    {
        return;
    }
    if (ceil && index == pgd_index(ceil))
    {
        return;
    }

    pud_free(pgd);
    pgd_entry_clear(pgd);
}

static void pgd_range_free(pgd_t* pgd, uintptr_t start, uintptr_t end, uintptr_t floor, uintptr_t ceil, bool free_pages)
{
    uintptr_t next;

    pgd = pgd_offset(pgd, start);

    for (uintptr_t vaddr = start; vaddr != end; vaddr = next, pgd++)
    {
        next = pgd_addr_next(vaddr, end);

        if (pgd_entry_none(pgd))
        {
            continue;
        }

        pud_range_free(pgd, vaddr, next, floor, ceil, free_pages);
    }
}

static void vm_remove_impl(vm_area_t* vma, uintptr_t start, uintptr_t end, pgd_t* pgd, bool free_pages)
{
    uintptr_t floor = vma->prev ? vma->prev->end - 1 : 0;
    uintptr_t ceil = vma->next ? vma->next->start : 0;

    pgd_range_free(pgd, start, end, floor, ceil, free_pages);

#if CONFIG_SEGMEXEC
    if (vma->vm_flags & VM_EXEC)
    {
        start += CODE_START;
        end += CODE_START;
        if (vma->prev && vma->prev->vm_flags & VM_EXEC)
        {
            floor += CODE_START;
        }
        else
        {
            floor = 0;
        }
        if (vma->next && vma->next->vm_flags & VM_EXEC)
        {
            ceil += CODE_START;
        }
        else
        {
            ceil = 0;
        }
        pgd_range_free(pgd, start, end, floor, ceil, false);
    }
#endif
}

static void vm_unmap_range_impl(
    vm_area_t* vma,
    uintptr_t start,
    uintptr_t end,
    pgd_t* pgd)
{
    // FIXME: there's shouldn't be any irq lock
    scoped_irq_lock();

    vm_remove_impl(vma, start, end, pgd, !(vma->vm_flags & VM_IO));
}

int vm_unmap_range(vm_area_t* vma, uintptr_t start, uintptr_t end, pgd_t* pgd)
{
    vm_unmap_range_impl(vma, start, end, pgd);
    return 0;
}

int vm_unmap(vm_area_t* vma, pgd_t* pgd)
{
    vm_unmap_range_impl(vma, vma->start, vma->end, pgd);
    return 0;
}

int vm_free(vm_area_t* vma_list, pgd_t* pgd)
{
    vm_area_t* temp;
    bool free_pages;
    uintptr_t start, end, next_start;

    for (vm_area_t* vma = vma_list; vma;)
    {
        if (!(vma->vm_flags & VM_IO))
        {
            free_pages = true;
        }
        else
        {
            free_pages = false;
        }

        start = vma->start;
        end = vma->end;
        next_start = vma->next ? vma->next->start : 0;
        pgd_range_free(pgd, start, end, 0, next_start, free_pages);

#if CONFIG_SEGMEXEC
        if (vma->vm_flags & VM_EXEC)
        {
            start += CODE_START;
            end += CODE_START;
            if (next_start && vma->next->vm_flags & VM_EXEC)
            {
                next_start += CODE_START;
            }
            else
            {
                next_start = 0;
            }
            pgd_range_free(pgd, start, end, 0, next_start, false);
        }
#endif

        temp = vma;
        vma = vma->next;
        delete(temp);
    }

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

int vm_verify_impl(vm_verify_flag_t verify, uintptr_t vaddr, size_t size, const vm_area_t* vma)
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

static int vm_verify_string_impl(vm_verify_flag_t flag, const char* string, size_t limit, const vm_area_t* vma)
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

int vm_verify_string(vm_verify_flag_t flag, const char* string, const vm_area_t* vma)
{
    return vm_verify_string_impl(flag, string, -1, vma);
}

int vm_verify_string_limit(vm_verify_flag_t flag, const char* string, size_t limit, const vm_area_t* vma)
{
    return vm_verify_string_impl(flag, string, limit, vma);
}
