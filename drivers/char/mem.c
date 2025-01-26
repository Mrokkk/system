#include <arch/io.h>

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/devfs.h>
#include <kernel/string.h>
#include <kernel/module.h>
#include <kernel/page_alloc.h>
#include <kernel/page_table.h>

#define MINOR_NULL 1
#define MINOR_MEM  2

KERNEL_MODULE(mem);
module_init(mem_init);
module_exit(mem_deinit);

static int mem_open(file_t* file);
static int null_write(file_t* file, const char* buffer, size_t size);
static int null_read(file_t* file, char* buffer, size_t count);
static int mem_mmap(file_t* file, vm_area_t* vma);
static int mem_nopage(vm_area_t* vma, uintptr_t address, size_t size, page_t** page);

static file_operations_t fops = {
    .open = &mem_open,
};

static vm_operations_t mem_vmops = {
    .nopage = &mem_nopage,
};

UNMAP_AFTER_INIT static int mem_init()
{
    int errno;

    if (unlikely(errno = devfs_register("null", MAJOR_CHR_MEM, MINOR_NULL, &fops)))
    {
        log_error("failed to register null to devfs: %d", errno);
        return errno;
    }

    if (unlikely(errno = devfs_register("mem", MAJOR_CHR_MEM, MINOR_MEM, &fops)))
    {
        log_error("failed to register null to devfs: %d", errno);
        return errno;
    }

    return 0;
}

static int mem_deinit()
{
    return 0;
}

static int mem_open(file_t* file)
{
    switch (MINOR(file->dentry->inode->rdev))
    {
        case MINOR_NULL:
            file->ops->read = &null_read;
            file->ops->write = &null_write;
            break;
        case MINOR_MEM:
            file->ops->mmap = &mem_mmap;
            break;
        default:
            return -ENODEV;
    }

    return 0;
}

static int null_write(file_t*, const char*, size_t)
{
    return 0;
}

static int null_read(file_t*, char*, size_t)
{
    return 0;
}

static int mem_mmap(file_t*, vm_area_t* vma)
{
    if (vma->vm_flags & (VM_WRITE | VM_EXEC))
    {
        return -EPERM;
    }
    vma->ops = &mem_vmops;
    return 0;
}

static int mem_nopage(vm_area_t* vma, uintptr_t address, size_t, page_t** page)
{
    size_t vma_off = address - vma->start;
    uintptr_t paddr = vma->offset + vma_off;
    page_t* src_page = page(paddr);
    page_t* dest_page = page_alloc(1, 0);

    if (unlikely(!dest_page))
    {
        return -ENOMEM;
    }

    page_kernel_map(src_page, kernel_identity_pgprot(PAGE_ALLOC_UNCACHED));
    memcpy(page_virt_ptr(dest_page), page_virt_ptr(src_page), PAGE_SIZE);
    page_kernel_unmap(src_page);

    *page = dest_page;

    return PAGE_SIZE;
}
