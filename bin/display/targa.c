#include "targa.h"

#include <sys/ioctl.h>

#include "utils.h"

static void pixel_set(uint32_t x, uint32_t y, uint32_t color, uint32_t pitch, uint8_t* dest)
{
    uint32_t* pixel = (uint32_t*)(dest + y * pitch + x * 4);
    *pixel = color;
}

int tga_to_framebuffer(
    struct tga_header* base,
    int x,
    int y,
    struct fb_var_screeninfo* vinfo,
    uint8_t* framebuffer)
{
    int img_index = 0;
    uint32_t* img = ptr(addr(base) + sizeof(*base));

    if (base->magic1 != 0 || base->colormap != 0 ||
        base->encoding != 2 || base->cmaporig != 0 ||
        base->cmapent != 0 || base->x != 0 ||
        base->bpp != 32 || base->pixel_ordering != 2)
    {
        return -1;
    }

    for (int j = 0; j < base->h; ++j)
    {
        if (y + j >= (int)vinfo->yres)
        {
            break;
        }
        for (int i = 0; i < base->w; ++i, ++img_index)
        {
            // FIXME: dummy handling of alpha channel
            if ((img[img_index] >> 24) < 0x80)
            {
                continue;
            }
            if (x + i >= (int)vinfo->xres)
            {
                continue;
            }
            pixel_set(x + i, y + j, img[img_index], vinfo->pitch, framebuffer);
        }
    }

    return 0;
}
