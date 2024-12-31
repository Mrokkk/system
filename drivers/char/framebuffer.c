#include "framebuffer.h"

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/devfs.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/api/ioctl.h>

#include <arch/multiboot.h>

static int framebuffer_open();
static int framebuffer_write(file_t*, const char* data, size_t size);
static int framebuffer_mmap(file_t* file, vm_area_t* vma);
static int framebuffer_ioctl(file_t* file, unsigned long request, void* arg);
static int framebuffer_nopage(vm_area_t* vma, uintptr_t address, size_t size, page_t** page);

framebuffer_t framebuffer;

static file_operations_t fops = {
    .open = &framebuffer_open,
    .write = &framebuffer_write,
    .mmap = &framebuffer_mmap,
    .ioctl = &framebuffer_ioctl,
};

static vm_operations_t vmops = {
    .nopage = &framebuffer_nopage,
};

module_init(framebuffer_init);
module_exit(framebuffer_deinit);
KERNEL_MODULE(framebuffer);

UNMAP_AFTER_INIT static int framebuffer_init()
{
    uint8_t* fb = ptr(addr(framebuffer_ptr->addr));
    size_t pitch = framebuffer_ptr->pitch;
    size_t width = framebuffer_ptr->width;
    size_t height = framebuffer_ptr->height;
    size_t bpp = framebuffer_ptr->bpp;
    size_t size = pitch * height;

    if (framebuffer_ptr->type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
    {
        devfs_register("fb0", MAJOR_CHR_FB, 0, &fops);
        framebuffer.fb = mmio_map(addr(fb), pitch * height, "framebuffer");
    }
    else
    {
        framebuffer.fb = mmio_map(addr(fb), page_align(2 * pitch * height), "framebuffer");
    }

    log_notice("framebuffer: %x addr = %x, resolution = %ux%u, pitch = %x, bpp=%u, size = %x",
        fb, framebuffer.fb, width, height, pitch, bpp, size);

    framebuffer.size = size;
    framebuffer.pitch = pitch;
    framebuffer.width = width;
    framebuffer.height = height;
    framebuffer.bpp = bpp;
    framebuffer.type = framebuffer_ptr->type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB
        ? FB_TYPE_RGB
        : FB_TYPE_TEXT;

    return 0;
}

static int framebuffer_deinit()
{
    return 0;
}

static int framebuffer_open(file_t*)
{
    return 0;
}

static int framebuffer_write(file_t*, const char* data, size_t size)
{
    memcpy(framebuffer.fb, data, size);
    return size;
}

static int framebuffer_mmap(file_t*, vm_area_t* vma)
{
    vma->vm_flags |= VM_IO;
    vma->ops = &vmops;
    return 0;
}

static int framebuffer_ioctl(file_t*, unsigned long request, void* arg)
{
    int errno;
    fb_var_screeninfo_t* vinfo = arg;

    if ((errno = current_vm_verify(VERIFY_WRITE, vinfo)))
    {
        return errno;
    }

    switch (request)
    {
        case FBIOGET_VSCREENINFO:
            vinfo->xres = framebuffer.width;
            vinfo->yres = framebuffer.height;
            vinfo->bits_per_pixel = framebuffer.bpp;
            vinfo->pitch = framebuffer.pitch;
            return 0;
        default: return -EINVAL;
    }
}

static int framebuffer_nopage(vm_area_t* vma, uintptr_t address, size_t, page_t** page)
{
    uintptr_t paddr = (address - vma->start) + addr(framebuffer_ptr->addr);
    *page = page(paddr);
    return PAGE_SIZE;
}
