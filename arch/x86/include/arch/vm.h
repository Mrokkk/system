#pragma once

#include <stddef.h>
#include <kernel/page.h>
#include <kernel/errno.h>
#include <kernel/compiler.h>

#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008
#define VM_EXECUTABLE   0x00001000
#define VM_NONFREEABLE  0x00010000

typedef struct pages
{
    struct list_head head;
    int count;
} pages_t;

// Currently supports max 4M areas
typedef struct vm_area
{
    pages_t* pages;
    uint32_t virt_address;
    uint32_t size;
    int vm_flags;
    struct vm_area* next;
} vm_area_t;

#define VM_AREA_INIT() {NULL, 0, 0, 0, NULL}

vm_area_t* vm_create(page_t* page_range, uint32_t virt_address, uint32_t size, int vm_flags);
vm_area_t* vm_extend(vm_area_t* vma, page_t* page_range, int vm_flags, int count); // FIXME: count is not needed
vm_area_t* vm_find(uint32_t virt_address, vm_area_t* areas);

int vm_add(vm_area_t** head, vm_area_t* new_vma);

#define VM_APPLY_DONT_REPLACE   0
#define VM_APPLY_REPLACE_PG     1

int vm_apply(vm_area_t* vma, pgd_t* pgd, int vm_apply_flags);
int vm_remove(vm_area_t* vma, pgd_t* pgd, int remove_pte);
int vm_copy(vm_area_t* dest_vma, const vm_area_t* src_vma, pgd_t* dest_pgd, const pgd_t* src_pgd);
int vm_copy_on_write(vm_area_t* vma);

void vm_print(const vm_area_t* vma);

static inline uint32_t pde_flags_get(int)
{
    uint32_t pgt_flags = PDE_PRESENT | PDE_USER | PDE_WRITEABLE;
    return pgt_flags;
}

static inline uint32_t pte_flags_get(int vm_flags)
{
    uint32_t pg_flags = PTE_PRESENT | PTE_USER;
    if (vm_flags & VM_WRITE) pg_flags |= PTE_WRITEABLE;
    return pg_flags;
}

static inline uint32_t vm_virt_end(const vm_area_t* a)
{
    return a->virt_address + a->size;
}

static inline uint32_t vm_paddr(uint32_t vaddr, const pgd_t* pgd)
{
    uint32_t offset = vaddr - page_beginning(vaddr);
    uint32_t pde_index = pde_index(vaddr);
    uint32_t pte_index = pte_index(vaddr);
    if (!pgd[pde_index])
    {
        return 0;
    }

    pgt_t* pgt = virt_ptr(pgd[pde_index] & ~PAGE_MASK);
    return (pgt[pte_index] & ~PAGE_MASK) + offset;
}

static inline uint32_t vm_paddr_end(vm_area_t* a, pgd_t* pgd)
{
    uint32_t vaddr = page_beginning(a->virt_address + a->size - 1);
    uint32_t pde_index = pde_index(vaddr);
    uint32_t pte_index = pte_index(vaddr);
    if (!pgd[pde_index])
    {
        return 0;
    }

    pgt_t* pgt = virt_ptr(pgd[pde_index] & ~PAGE_MASK);
    return (pgt[pte_index] & ~PAGE_MASK) + PAGE_SIZE;
}

#define VERIFY_READ     1
#define VERIFY_WRITE    2

int __vm_verify(char verify, uint32_t vaddr, size_t size, vm_area_t* vma);

static inline int vm_verify(char verify, const void* ptr, size_t size, vm_area_t* vma)
{
    if (!vma)
    {
        // FIXME: hack for kernel processes
        return 0;
    }

    if (unlikely(kernel_address(addr(ptr))))
    {
        return -EFAULT;
    }

    return __vm_verify(verify, addr(ptr), size, vma);
}

#define vm_print_single(vma, flag) \
    do { \
        page_t* p; \
        int safety = 0; \
        log_debug(flag, "vm_area={%x, vaddr={%x-%x}, size=%x, pages={count=%u", \
            vma, \
            vma->virt_address, \
            vma->virt_address + vma->size - 1, \
            vma->size, \
            vma->pages->count); \
        list_for_each_entry(p, &vma->pages->head, list_entry) \
        { \
            log_debug(flag, "\t\tpage: %x", page_phys(p)); \
            if (++safety > 100) { panic("infinite loop detection"); } \
        } \
        log_debug(flag, "}"); \
    } while(0)
