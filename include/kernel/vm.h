#pragma once

#include <kernel/list.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

struct inode;

typedef struct pages pages_t;
typedef struct vm_area vm_area_t;

#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008
#define VM_IO           0x00000010
#define VM_COW          0x00000020
#define VM_TYPE_MASK    0xff000000

#define VM_TYPE_STACK   1
#define VM_TYPE_HEAP    2

#define VM_TYPE(type)    ((type) << 24)
#define VM_TYPE_GET(val) ((val) >> 24)

struct vm_area
{
    pages_t*      pages;
    uint32_t      start, end;
    int           vm_flags;
    struct inode* inode;
    uint32_t      offset; // offset within inode
    vm_area_t*    next;
    vm_area_t*    prev;
};

struct pages
{
    list_head_t   head;
    int           refcount;
};

#define VM_APPLY_EXTEND 2

vm_area_t* vm_create(uint32_t virt_address, uint32_t size, int vm_flags);
vm_area_t* vm_find(uint32_t virt_address, vm_area_t* areas);

int vm_add(vm_area_t** head, vm_area_t* new_vma);
void vm_del(vm_area_t* vma);
int vm_map(vm_area_t* vma, page_t* page_range, pgd_t* pgd, int vm_apply_flags);
int vm_remap(vm_area_t* vma, page_t* page_range, pgd_t* pgd);
int vm_unmap(vm_area_t* vma, pgd_t* pgd);
int vm_free(vm_area_t* vma_list, pgd_t* pgd);
int vm_copy(vm_area_t* dest_vma, const vm_area_t* src_vma, pgd_t* dest_pgd, const pgd_t* src_pgd);
int vm_copy_on_write(vm_area_t* vma, const pgd_t* pgd);
int vm_io_apply(vm_area_t* vma, pgd_t* pgd, uint32_t start);

typedef enum
{
    VERIFY_READ     = 1,
    VERIFY_WRITE    = 2
} vm_verify_flag_t;

static inline int _vm_verify(vm_verify_flag_t flag, const void* ptr, size_t size, vm_area_t* vma)
{
    extern int __vm_verify(vm_verify_flag_t flag, uint32_t vaddr, size_t size, vm_area_t* vma);

    if (!vma)
    {
        // FIXME: hack for kernel processes
        return 0;
    }

    if (unlikely(kernel_address(addr(ptr))))
    {
        return -EFAULT;
    }

    return __vm_verify(flag, addr(ptr), size, vma);
}

// vm_verify_string - check access to the string pointed by string
//
// @flag - vm_verify_flag_t as defined above
// @string - pointer to a string
// @vma - vm_areas against which access is checked
int vm_verify_string(vm_verify_flag_t flag, const char* string, vm_area_t* vma);

// vm_verify - check access to the object pointed by data_ptr
//
// @flag - vm_verify_flag_t as defined above
// @data_ptr - pointer to data
// @vma - vm_areas against which access is checked
#define vm_verify(flag, data_ptr, vma) \
    ({ _vm_verify(flag, data_ptr, sizeof(*(data_ptr)), vma); })

// vm_verify_array - check access to the array of n objects pointed by data_ptr
//
// @flag - vm_verify_flag_t as defined above
// @data_ptr - pointer to data
// @n - size of array
// @vma - vm_areas against which access is checked
#define vm_verify_array(flag, data_ptr, n, vma) \
    ({ _vm_verify(flag, data_ptr, sizeof(*(data_ptr)) * (n), vma); })

// vm_verify_buf - check access to the buffer of n bytes pointed by data_ptr
//
// @flag - vm_verify_flag_t as defined above
// @data_ptr - pointer to data
// @n - number of bytes
// @vma - vm_areas against which access is checked
#define vm_verify_buf(flag, data_ptr, n, vma) \
    ({ _vm_verify(flag, data_ptr, n, vma); })

#define vm_for_each(vma, vm_areas) \
    for (vma = vm_areas; vma; vma = vma->next)

#include <arch/vm.h>

int arch_vm_apply(pgd_t* pgd, page_t* pages, uint32_t start, uint32_t end, int vm_flags);
int arch_vm_reapply(pgd_t* pgd, page_t* pages, uint32_t start, uint32_t end, int vm_flags);
int arch_vm_copy(pgd_t* dest_pgd, const pgd_t* src_pgd,  uint32_t start, uint32_t end);
int arch_vm_copy_on_write(vm_area_t* vma, const pgd_t* pgd, page_t* pages);
