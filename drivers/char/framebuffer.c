#define log_fmt(fmt) "framebuffer: " fmt
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/list.h>
#include <kernel/devfs.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/api/ioctl.h>
#include <kernel/framebuffer.h>

static int framebuffer_open(file_t* file);
static int framebuffer_write(file_t* file, const char* data, size_t size);
static int framebuffer_mmap(file_t* file, vm_area_t* vma);
static int framebuffer_ioctl(file_t* file, unsigned long request, void* arg);
static int framebuffer_nopage(vm_area_t* vma, uintptr_t address, size_t size, page_t** page);

struct fb_client
{
    void* data;
    list_head_t list_entry;
    void (*callback)(void* data);
};

typedef struct fb_client fb_client_t;

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

static LIST_DECLARE(fb_clients);

module_init(framebuffer_init);
module_exit(framebuffer_deinit);
KERNEL_MODULE(framebuffer);

UNMAP_AFTER_INIT static int framebuffer_init(void)
{
    int errno;
    uintptr_t paddr = framebuffer.paddr;
    size_t pitch = framebuffer.pitch;
    size_t width = framebuffer.width;
    size_t height = framebuffer.height;
    size_t bpp = framebuffer.bpp;
    size_t size = framebuffer.size;

    log_notice("%x addr = %x, resolution = %ux%u, pitch = %x, bpp=%u, size = %x",
        paddr, framebuffer.vaddr, width, height, pitch, bpp, size);

    if (unlikely(errno = devfs_register("fb0", MAJOR_CHR_FB, 0, &fops)))
    {
        log_error("failed to register device: %d", errno);
        return errno;
    }

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
    memcpy(framebuffer.vaddr, data, size);
    return size;
}

static int framebuffer_mmap(file_t*, vm_area_t* vma)
{
    vma->vm_flags |= VM_IO;
    vma->ops = &vmops;
    return 0;
}

static void framebuffer_fb_clients_notify(void)
{
    fb_client_t* c;

    list_for_each_entry(c, &fb_clients, list_entry)
    {
        c->callback(c->data);
    }
}

static int framebuffer_ioctl(file_t*, unsigned long request, void* arg)
{
    int errno;
    fb_var_screeninfo_t* vinfo = arg;
    fb_fix_screeninfo_t* finfo = arg;

    switch (request)
    {
        case FBIOGET_FSCREENINFO:
            if (unlikely(errno = current_vm_verify(VERIFY_WRITE, finfo)))
            {
                return errno;
            }

            strlcpy(finfo->id, framebuffer.id, sizeof(finfo->id));
            finfo->smem_start  = framebuffer.paddr;
            finfo->smem_len    = framebuffer.size;
            finfo->type        = framebuffer.type;
            finfo->type_aux    = framebuffer.type_aux;
            finfo->visual      = framebuffer.visual;
            finfo->line_length = framebuffer.pitch;
            finfo->accel       = framebuffer.accel;

            return 0;

        case FBIOGET_VSCREENINFO:
            if (unlikely(errno = current_vm_verify(VERIFY_WRITE, vinfo)))
            {
                return errno;
            }

            vinfo->xres = framebuffer.width;
            vinfo->yres = framebuffer.height;
            vinfo->bits_per_pixel = framebuffer.bpp;

            return 0;

        case FBIOPUT_VSCREENINFO:
            if (unlikely(errno = current_vm_verify(VERIFY_READ, vinfo)))
            {
                return errno;
            }

            if (unlikely(!framebuffer.ops || !framebuffer.ops->mode_get || !framebuffer.ops->mode_set))
            {
                return -ENOSYS;
            }

            int mode = framebuffer.ops->mode_get(vinfo->xres, vinfo->yres, vinfo->bits_per_pixel);

            if (unlikely(mode < 0))
            {
                return mode;
            }

            if (unlikely(errno = framebuffer.ops->mode_set(mode)))
            {
                return errno;
            }

            framebuffer_fb_clients_notify();

            return 0;

        default:
            return -EINVAL;
    }
}

static int framebuffer_nopage(vm_area_t* vma, uintptr_t address, size_t, page_t** page)
{
    uintptr_t paddr = (address - vma->start) + framebuffer.paddr;

    if (unlikely(paddr >= framebuffer.paddr + framebuffer.size))
    {
        return -EFAULT;
    }

    *page = page(paddr);
    return PAGE_SIZE;
}

int framebuffer_client_register(void (*callback)(void* data), void* data)
{
    fb_client_t* c = alloc(fb_client_t);

    if (unlikely(!c))
    {
        return -ENOMEM;
    }

    list_init(&c->list_entry);
    c->data = data;
    c->callback = callback;

    list_add(&c->list_entry, &fb_clients);

    return 0;
}
