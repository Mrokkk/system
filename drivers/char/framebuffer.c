#include "framebuffer.h"

#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/page.h>
#include <kernel/devfs.h>
#include <kernel/ioctl.h>
#include <kernel/device.h>
#include <kernel/process.h>

#include <arch/multiboot.h>

static int framebuffer_open();
static int framebuffer_write(file_t*, const char* data, size_t size);
static int framebuffer_mmap(file_t* file, vm_area_t* vma, size_t offset);
static int framebuffer_ioctl(file_t* file, unsigned long request, void* arg);

framebuffer_t framebuffer;

static file_operations_t fops = {
    .open = &framebuffer_open,
    .write = &framebuffer_write,
    .mmap = &framebuffer_mmap,
    .ioctl = &framebuffer_ioctl,
};

module_init(framebuffer_init);
module_exit(framebuffer_deinit);
KERNEL_MODULE(framebuffer);

UNMAP_AFTER_INIT int framebuffer_init()
{
    uint8_t* fb = ptr((uint32_t)framebuffer_ptr->addr);
    uint32_t pitch = framebuffer_ptr->pitch;
    uint32_t width = framebuffer_ptr->width;
    uint32_t height = framebuffer_ptr->height;
    uint32_t bpp = framebuffer_ptr->bpp;
    uint32_t size = pitch * height;

    if (framebuffer_ptr->type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
    {
        char_device_register(MAJOR_CHR_FB, "fb", &fops, 0, NULL);
        devfs_register("fb0", MAJOR_CHR_FB, 0);
        framebuffer.fb = region_map(addr(fb), pitch * height, "framebuffer");
    }
    else
    {
        framebuffer.fb = region_map(addr(fb), page_align(2 * pitch * height), "framebuffer");
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

int framebuffer_deinit()
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

static int framebuffer_mmap(file_t*, vm_area_t* vma, size_t)
{
    return vm_io_apply(vma, process_current->mm->pgd, framebuffer_ptr->addr);
}

static int framebuffer_ioctl(file_t*, unsigned long request, void* arg)
{
    int errno;
    fb_var_screeninfo_t* vinfo = arg;

    if ((errno = vm_verify(VERIFY_WRITE, vinfo, sizeof(fb_var_screeninfo_t), process_current->mm->vm_areas)))
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
