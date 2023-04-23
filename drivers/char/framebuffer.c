#include "framebuffer.h"

#include <kernel/fs.h>
#include <kernel/font.h>
#include <kernel/page.h>
#include <kernel/device.h>

#include <arch/multiboot.h>

int framebuffer_open();
int framebuffer_write();

framebuffer_t framebuffer;

static struct file_operations fops = {
    .open = &framebuffer_open,
    .write = &framebuffer_write,
};

module_init(framebuffer_init);
module_exit(framebuffer_deinit);
KERNEL_MODULE(framebuffer);

struct tga_header
{
    uint8_t magic1;
    uint8_t colormap;
    uint8_t encoding;
    uint16_t cmaporig, cmaplen;
    uint8_t cmapent;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint8_t bpp;
    uint8_t pixeltype;
} __packed;

static inline void pixel_set(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t* pixel = (uint32_t*)(framebuffer.fb + y * framebuffer.pitch + x * 4);
    *pixel = color;
}

static void img_draw(
    int x,
    int y,
    int width,
    int height,
    uint32_t* img)
{
   int img_index = 0;

   for (int j = 0; j < height; ++j)
   {
      for (int i = 0; i < width; ++i, ++img_index)
      {
         pixel_set(x + i, y + j, img[img_index]);
      }
   }
}

void display_tga(struct tga_header *base)
{
    if (base->magic1 != 0 || base->colormap != 0 ||
        base->encoding != 2 || base->cmaporig != 0 ||
        base->cmapent != 0 || base->x != 0 ||
        base->bpp != 32 || base->pixeltype != 40)
   {
        log_warning("unsupported format!");
        return;
   }

   uint32_t* img = ptr(addr(base) + sizeof(struct tga_header));
   img_draw(500, 0, base->w, base->h, img);
}

int framebuffer_init()
{
    uint32_t pitch = framebuffer_ptr->pitch;
    uint32_t width = framebuffer_ptr->width;
    uint32_t height = framebuffer_ptr->height;
    uint32_t bpp = framebuffer_ptr->bpp;
    uint32_t size = pitch * height;

    char_device_register(MAJOR_CHR_FB, "fb", &fops, 0, NULL);

    // Map framebuffer memory in kernel
    page_kernel_identity_map(framebuffer_ptr->addr, pitch * height);
    uint8_t* fb = ptr((uint32_t)framebuffer_ptr->addr);

    log_notice("vaddr = paddr = %x", fb);
    log_notice("resolution = %ux%u, pitch = %x, bpp=%u, size = %x",
        width,
        height,
        pitch,
        bpp,
        size);

    framebuffer.fb = fb;
    framebuffer.size = size;
    framebuffer.pitch = pitch;
    framebuffer.width = width;
    framebuffer.height = height;
    framebuffer.bpp = bpp;
    framebuffer.type = framebuffer_ptr->type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB
        ? FB_TYPE_RGB
        : FB_TYPE_TEXT;

    if (framebuffer_ptr->type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
    {
        display_tga(tux);
    }

    return 0;
}

int framebuffer_deinit()
{
    return 0;
}

int framebuffer_open(struct file*)
{
    return 0;
}

int framebuffer_write(struct file*, char* data, int size)
{
    memcpy(framebuffer.fb, data, size);
    return size;
}
