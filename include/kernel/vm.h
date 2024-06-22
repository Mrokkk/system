#pragma once

#include <kernel/page.h>
#include <kernel/kernel.h>

struct inode;
struct pages;
struct vm_area;

typedef struct pages pages_t;
typedef struct vm_area vm_area_t;

#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008
#define VM_IO           0x00000010
#define VM_EXECUTABLE   0x00001000
#define VM_NONFREEABLE  0x00010000

struct vm_area
{
    pages_t* pages;
    uint32_t start, end;
    int vm_flags;
    struct inode* inode;
    vm_area_t* next;
    vm_area_t* prev;
};

struct pages
{
    list_head_t head;
    int count;
};

#define VM_AREA_INIT() {NULL, 0, 0, 0, NULL}

#define VM_APPLY_DONT_REPLACE   0
#define VM_APPLY_EXTEND         2

vm_area_t* vm_create(uint32_t virt_address, uint32_t size, int vm_flags);
vm_area_t* vm_find(uint32_t virt_address, vm_area_t* areas);

int vm_add(vm_area_t** head, vm_area_t* new_vma);
int vm_map(vm_area_t* vma, page_t* page_range, pgd_t* pgd, int vm_apply_flags);
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

static inline char* vm_flags_string(char* buffer, int vm_flags)
{
    char* b = buffer;
    b += sprintf(b, (vm_flags & VM_READ) ? "R" : "-");
    b += sprintf(b, (vm_flags & VM_WRITE) ? "W" : "-");
    b += sprintf(b, (vm_flags & VM_EXEC) ? "X" : "-");
    b += sprintf(b, (vm_flags & VM_EXEC) ? "N" : "-");
    return buffer;
}

#define vm_print_single(vma, flag) \
    if (flag) \
    { \
        do { \
            page_t* p; \
            char buf[5]; \
            int safety = 0; \
            log_debug(flag, "%x:[%08x-%08x %08x %s i=%08x p=%u]", \
                vma, \
                vma->start, \
                vma->end, \
                vma->end - vma->start, \
                vm_flags_string(buf, vma->vm_flags), \
                vma->inode, \
                vma->pages->count); \
            list_for_each_entry(p, &vma->pages->head, list_entry) \
            { \
                log_debug(flag, "  page: %x[%u]", page_phys(p), pfn(p)); \
                if (++safety > 1024 * 1024) { panic("infinite loop detection"); } \
            } \
        } while(0); \
    }

#define vm_print(vma, flag) \
    { \
        if (flag && vma) \
        { \
            for (const vm_area_t* temp = vma; temp; temp = temp->next) \
            { \
                vm_print_single(temp, flag); \
            } \
        } \
    }

#define vm_print_short(orig_vma, flag) \
    if (flag) \
    { \
        do { \
            for (vm_area_t* vma = orig_vma; vma; vma = vma->next) \
            { \
                char buf[5]; \
                log_debug(flag, "%x:[%08x-%08x %08x %s i=%08x p=%u]", \
                    vma, \
                    vma->start, \
                    vma->end, \
                    vma->end - vma->start, \
                    vm_flags_string(buf, vma->vm_flags), \
                    vma->inode, \
                    vma->pages->count); \
            } \
        } while(0); \
    }

#include <arch/vm.h>

int arch_vm_apply(pgd_t* pgd, page_t* pages, uint32_t start, uint32_t end, int vm_flags);
int arch_vm_copy(pgd_t* dest_pgd, const pgd_t* src_pgd,  uint32_t start, uint32_t end);
int arch_vm_copy_on_write(vm_area_t* vma, const pgd_t* pgd, page_t* pages);
