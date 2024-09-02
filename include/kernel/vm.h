#pragma once

#include <stdbool.h>
#include <kernel/list.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

struct dentry;

typedef struct pages pages_t;
typedef struct vm_area vm_area_t;
typedef struct vm_operations vm_operations_t;

#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008
#define VM_IO           0x00000010
#define VM_IMMUTABLE    0x00000020
#define VM_TYPE_MASK    0xff000000

#define VM_TYPE_STACK   1
#define VM_TYPE_HEAP    2

#define VM_TYPE(type)    ((type) << 24)
#define VM_TYPE_GET(val) ((val) >> 24)

struct vm_operations
{
    int (*nopage)(vm_area_t* vma, uintptr_t address, size_t size, page_t** page);
};

struct vm_area
{
    uintptr_t        start, end;
    uintptr_t        actual_end;
    int              vm_flags;
    struct dentry*   dentry;
    off_t            offset; // offset within inode
    vm_operations_t* ops;
    vm_area_t*       next;
    vm_area_t*       prev;
};

vm_area_t* vm_create(uint32_t virt_address, size_t size, int vm_flags);
vm_area_t* vm_find(uint32_t virt_address, vm_area_t* areas);

int vm_add(vm_area_t** head, vm_area_t* new_vma);
void vm_add_tail(vm_area_t* new_vma, vm_area_t* old_vma);
void vm_add_front(vm_area_t* new_vma, vm_area_t* old_vma);
void vm_del(vm_area_t* vma);
void vm_areas_del(vm_area_t* vmas);
int vm_unmap(vm_area_t* vma, pgd_t* pgd);
int vm_unmap_range(vm_area_t* vma, uintptr_t start, uintptr_t end, pgd_t* pgd);
int vm_free(vm_area_t* vma_list, pgd_t* pgd);
int vm_copy(vm_area_t* dest_vma, const vm_area_t* src_vma, pgd_t* dest_pgd, pgd_t* src_pgd);
int vm_io_apply(vm_area_t* vma, pgd_t* pgd, uint32_t start);
int vm_nopage(vm_area_t* vma, pgd_t* pgd, uintptr_t address, bool write);

// vm_replace - replace vm areas <replace_start, replace_end> with <new_vmas, new_vmas_end>
void vm_replace(
    vm_area_t** vm_areas,
    vm_area_t* new_vmas,
    vm_area_t* new_vmas_end,
    vm_area_t* replace_start,
    vm_area_t* replace_end);

// vm_apply - apply protection flags to pages in range <vaddr_start, vaddr_end)
//
// @vmas - list of vmas from which protection flags are taken
// @pgd - pgd to which change is applied
// @vaddr_start - beginning of virtual address space
// @vaddr_end - end of virtual address space
int vm_apply(vm_area_t* vmas, pgd_t* pgd, uintptr_t vaddr_start, uintptr_t vaddr_end);

static inline bool address_within(uint32_t vaddr, vm_area_t* vma)
{
    return vaddr >= vma->start && vaddr < vma->end;
}

typedef enum
{
    VERIFY_READ     = 1,
    VERIFY_WRITE    = 2,
} vm_verify_flag_t;

static inline int vm_verify_wrapper(vm_verify_flag_t flag, const void* ptr, size_t size, vm_area_t* vma)
{
    extern int vm_verify_impl(vm_verify_flag_t flag, uint32_t vaddr, size_t size, vm_area_t* vma);

    if (unlikely(kernel_address(addr(ptr))))
    {
        return -EFAULT;
    }

    return vm_verify_impl(flag, addr(ptr), size, vma);
}

// vm_verify_string - check access to the string pointed by string
//
// @flag - vm_verify_flag_t as defined above
// @string - pointer to a string
// @vma - vm_areas against which access is checked
int vm_verify_string(vm_verify_flag_t flag, const char* string, vm_area_t* vma);

// vm_verify_string - check access to the max limit bytes from string pointed by string
//
// @flag - vm_verify_flag_t as defined above
// @string - pointer to a string
// @limit - max number of bytes checked
// @vma - vm_areas against which access is checked
int vm_verify_string_limit(vm_verify_flag_t flag, const char* string, size_t limit, vm_area_t* vma);

// vm_verify - check access to the object pointed by data_ptr
//
// @flag - vm_verify_flag_t as defined above
// @data_ptr - pointer to data
// @vma - vm_areas against which access is checked
#define vm_verify(flag, data_ptr, vma) \
    ({ vm_verify_wrapper(flag, data_ptr, sizeof(*(data_ptr)), vma); })

// vm_verify_array - check access to the array of n objects pointed by data_ptr
//
// @flag - vm_verify_flag_t as defined above
// @data_ptr - pointer to data
// @n - size of array
// @vma - vm_areas against which access is checked
#define vm_verify_array(flag, data_ptr, n, vma) \
    ({ vm_verify_wrapper(flag, data_ptr, sizeof(*(data_ptr)) * (n), vma); })

// vm_verify_buf - check access to the buffer of n bytes pointed by data_ptr
//
// @flag - vm_verify_flag_t as defined above
// @data_ptr - pointer to data
// @n - number of bytes
// @vma - vm_areas against which access is checked
#define vm_verify_buf(flag, data_ptr, n, vma) \
    ({ vm_verify_wrapper(flag, data_ptr, n, vma); })

#define vm_for_each(vma, vm_areas) \
    for (vma = vm_areas; vma; vma = vma->next)

#include <arch/vm.h>

int arch_vm_copy(vm_area_t* dest_vma, pgd_t* dest_pgd, pgd_t* src_pgd,  uint32_t start, uint32_t end);
int arch_vm_map_single(pgd_t* pgd, uint32_t pde_index, uint32_t pte_index, page_t* page, int vm_flags);
