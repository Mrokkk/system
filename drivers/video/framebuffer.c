#define log_fmt(fmt) "framebuffer: " fmt
#include <kernel/fs.h>
#include <kernel/vm.h>
#include <kernel/list.h>
#include <kernel/devfs.h>
#include <kernel/mutex.h>
#include <kernel/timer.h>
#include <kernel/minmax.h>
#include <kernel/module.h>
#include <kernel/process.h>
#include <kernel/api/ioctl.h>
#include <kernel/framebuffer.h>

static int framebuffer_open(file_t* file);
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

READONLY static file_operations_t fops = {
    .open = &framebuffer_open,
    .mmap = &framebuffer_mmap,
    .ioctl = &framebuffer_ioctl,
};

READONLY static vm_operations_t vmops = {
    .nopage = &framebuffer_nopage,
};

static LIST_DECLARE(fb_clients);

module_init(framebuffer_init);
module_exit(framebuffer_deinit);
KERNEL_MODULE(framebuffer);

UNMAP_AFTER_INIT static int framebuffer_init(void)
{
    int errno;

    if (unlikely(errno = devfs_register("fb0", MAJOR_CHR_FB, 0, &fops)))
    {
        log_error("failed to register device: %d", errno);
        return errno;
    }

    mutex_init(&framebuffer.lock);

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

            if (framebuffer.id)
            {
                strlcpy(finfo->id, framebuffer.id, sizeof(finfo->id));
            }
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

            if (unlikely(!framebuffer.ops || !framebuffer.ops->mode_set))
            {
                return -ENOSYS;
            }

            if (unlikely(errno = framebuffer.ops->mode_set(vinfo->xres, vinfo->yres, vinfo->bits_per_pixel)))
            {
                return errno;
            }

            framebuffer_fb_clients_notify();

            return 0;

        default:
            return -EINVAL;
    }
}

static void framebuffer_callback(ktimer_t* timer)
{
    vm_area_t* vma;
    inode_t* inode = timer->data;

    list_for_each_entry(vma, &inode->mappings, mapping_entry)
    {
        vm_unmap_range(vma, vma->start, vma->end, vma->mm->pgd);
    }

    scoped_mutex_lock(&framebuffer.lock);

    framebuffer.timer = 0;

    framebuffer.ops->dirty_set(&framebuffer.dirty);

    fb_rect_clear(&framebuffer.dirty);
}

static fb_rect_t framebuffer_page_to_rect(uintptr_t paddr)
{
    fb_rect_t rect;

    size_t y0 = paddr / framebuffer.pitch;
    size_t rem = paddr % framebuffer.pitch;
    size_t x0 = rem / (framebuffer.bpp / 8);

    paddr += PAGE_SIZE;
    size_t y1 = align(paddr, framebuffer.pitch) / framebuffer.pitch;
    rem = paddr % framebuffer.pitch;
    size_t x1 = rem / (framebuffer.bpp / 8);

    if (x0 > x1)
    {
        rect.x = 0;
        rect.w = framebuffer.width;
    }
    else
    {
        rect.x = x0;
        rect.w = x1 - x0;
    }

    rect.y = y0;
    rect.h = y1 - y0;

    return rect;
}

static void framebuffer_dirty_rect_update(uintptr_t paddr)
{
    fb_rect_t rect = framebuffer_page_to_rect(paddr);
    fb_rect_enlarge(&framebuffer.dirty, &rect);
}

static void framebuffer_nopage_virt(vm_area_t* vma, uintptr_t offset)
{
    int errno;

    scoped_mutex_lock(&framebuffer.lock);
    framebuffer_dirty_rect_update(offset & ~PAGE_MASK);

    if (!framebuffer.timer)
    {
        framebuffer.timer = ktimer_create_and_start(
            KTIMER_ONESHOT,
            framebuffer.delay,
            &framebuffer_callback,
            vma->dentry->inode);

        if (unlikely(errno = errno_get(framebuffer.timer)))
        {
            log_error("cannot start timer: %d", errno);
            framebuffer.timer = 0;
        }
    }
}

static int framebuffer_nopage(vm_area_t* vma, uintptr_t address, size_t, page_t** page)
{
    uintptr_t offset = address - vma->start;
    uintptr_t paddr = offset + framebuffer.paddr;

    if (unlikely(paddr >= framebuffer.paddr + framebuffer.size))
    {
        return -EFAULT;
    }

    *page = page(paddr);

    if (framebuffer.flags & FB_FLAGS_VIRTFB)
    {
        framebuffer_nopage_virt(vma, offset & ~PAGE_MASK);
    }

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

void framebuffer_refresh(void)
{
    if (framebuffer.ops && framebuffer.ops->refresh)
    {
        framebuffer.ops->refresh();
    }
}
