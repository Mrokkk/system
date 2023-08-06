#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define addr(x) ((uint32_t)(x))
#define ptr(x)  ((void*)(x))

#define min(a, b) a < b ? a : b;

#define MOUSE_LEFT_BUTTON       1
#define MOUSE_RIGHT_BUTTON      2
#define MOUSE_MIDDLE_BUTTON     4

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
    uint8_t alpha_bits:4;
    uint8_t pixel_ordering:2;
    uint8_t reserved:2;
} __attribute__((packed));

uint8_t* framebuffer;
uint8_t* buffer;

static void pixel_set(uint32_t x, uint32_t y, uint32_t color, uint32_t pitch, uint8_t* dest)
{
    uint32_t* pixel = (uint32_t*)(dest + y * pitch + x * 4);
    *pixel = color;
}

static void img_draw(
    int x, int y,
    int width, int height,
    int xmax, int ymax,
    uint32_t* img,
    uint32_t pitch,
    uint8_t* dest)
{
    int img_index = 0;

    for (int j = 0; j < height; ++j)
    {
        if (y + j > ymax)
        {
            break;
        }
        for (int i = 0; i < width; ++i, ++img_index)
        {
            // FIXME: dummy handling of alpha channel
            if ((img[img_index] >> 24) < 0x80)
            {
                continue;
            }
            if (x + i > xmax)
            {
                continue;
            }
            pixel_set(x + i, y + j, img[img_index], pitch, dest);
        }
    }
}

#define BUFFERED 1
#define UNBUFFERED 2

static int display_tga(struct tga_header *base, struct fb_var_screeninfo* vinfo, int x, int y, int flag)
{
    uint32_t* img;

    if (base->magic1 != 0 || base->colormap != 0 ||
        base->encoding != 2 || base->cmaporig != 0 ||
        base->cmapent != 0 || base->x != 0 ||
        base->bpp != 32 || base->pixel_ordering != 2)
    {
        return -1;
    }

    int xmax = (int)vinfo->xres - 1;
    int ymax = (int)vinfo->yres - 1;

    img = ptr(addr(base) + sizeof(struct tga_header));

    if (flag == BUFFERED)
    {
        img_draw(x, y, base->w, base->h, xmax, ymax, img, vinfo->pitch, buffer);
        memcpy(framebuffer, buffer, vinfo->yres * vinfo->pitch);
    }
    else if (flag == UNBUFFERED)
    {
        img_draw(x, y, base->w, base->h, xmax, ymax, img, vinfo->pitch, framebuffer);
    }

    return 0;
}

static int cleanup()
{
    if (ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT))
    {
        perror("ioctl tty");
    }
    return 0;
}

static int sighan()
{
    cleanup();
    exit(EXIT_FAILURE);
    return 0;
}

static void* file_map(const char* path)
{
    int fd;
    struct stat s;
    void* res;

    stat(path, &s);
    fd = open(path, O_RDONLY, 0);

    if (fd == -1)
    {
        perror(path);
        return NULL;
    }

    res = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if ((int)res == -1)
    {
        perror(path);
        return NULL;
    }

    return res;
}

static int clamp(int value, int min, int max)
{
    if (value >= max)
    {
        value = max - 1;
    }
    else if (value < min)
    {
        value = 0;
    }
    return value;
}

int main(int argc, char* argv[])
{
    int fb_fd, mouse_fd;
    const char* img_path;
    struct tga_header* img;
    struct tga_header* cursor;
    struct fb_var_screeninfo vinfo;
    memset(&vinfo, 0, sizeof(vinfo));

    signal(SIGTERM, sighan);
    signal(SIGINT, sighan);
    signal(SIGTSTP, sighan);
    signal(SIGSEGV, sighan);

    if (argc < 2)
    {
        printf("display: no image path given\n");
        return EXIT_FAILURE;
    }

    img_path = argv[1];

    fb_fd = open("/dev/fb0", O_RDWR, 0);

    if (fb_fd == -1)
    {
        perror("/dev/fb0");
        return EXIT_FAILURE;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
    {
        perror("ioctl");
        return EXIT_FAILURE;
    }

    framebuffer = mmap(NULL, vinfo.yres * vinfo.pitch, PROT_READ | PROT_WRITE, MAP_PRIVATE, fb_fd, 0);
    if ((int)framebuffer == -1)
    {
        perror("mmap");
        return EXIT_FAILURE;
    }

    if (!(img = file_map(img_path)))
    {
        return EXIT_FAILURE;
    }

    /*printf("%u x %u x %u\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);*/
    /*printf("magic1 = %u\n", img->magic1);*/
    /*printf("colormap = %u\n", img->colormap);*/
    /*printf("encoding = %u\n", img->encoding);*/
    /*printf("cmaporig = %u\n", img->cmaporig);*/
    /*printf("cmaplen = %u\n", img->cmaplen);*/
    /*printf("cmapent = %u\n", img->cmapent);*/
    /*printf("x = %u\n", img->x);*/
    /*printf("y = %u\n", img->y);*/
    /*printf("w = %u\n", img->w);*/
    /*printf("h = %u\n", img->h);*/
    /*printf("bpp = %u\n", img->bpp);*/
    /*printf("pixeltype = %u\n", img->pixeltype);*/

    buffer = mmap(NULL, vinfo.yres * vinfo.pitch, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if ((int)buffer == -1)
    {
        perror("mmap buffer");
        return EXIT_FAILURE;
    }

    memset(buffer, 0, vinfo.yres * vinfo.pitch);

    if (!(cursor = file_map("/cursor.tga")))
    {
        return EXIT_FAILURE;
    }

    if (ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS))
    {
        perror("ioctl tty");
        return EXIT_FAILURE;
    }

    if (display_tga(img, &vinfo, 0, 0, BUFFERED))
    {
        cleanup();
        printf("%s: unsupported format\n", img_path);
        return EXIT_FAILURE;
    }

    mouse_fd = open("/dev/mouse", O_RDONLY, 0);

    if (mouse_fd == -1)
    {
        perror("/dev/mouse");
        cleanup();
        return EXIT_FAILURE;
    }

    char buf[3];
    int xpos = vinfo.xres / 2, ypos = vinfo.yres / 2;
    int old_xpos, old_ypos;

    display_tga(cursor, &vinfo, xpos, ypos, UNBUFFERED);

    while (1)
    {
        read(mouse_fd, buf, 3);

        if (buf[0] & MOUSE_LEFT_BUTTON)
        {
            break;
        }

        old_xpos = xpos;
        old_ypos = ypos;
        xpos += buf[1];
        ypos -= buf[2];

        xpos = clamp(xpos, 0, vinfo.xres);
        ypos = clamp(ypos, 0, vinfo.yres);

        for (int x = old_xpos; x < old_xpos + cursor->w; ++x)
        {
            if (x >= (int)vinfo.xres)
            {
                break;
            }
            for (int y = old_ypos; y < old_ypos + cursor->h; ++y)
            {
                if (y >= (int)vinfo.yres)
                {
                    break;
                }
                uint32_t* pixel = (uint32_t*)(framebuffer + y * vinfo.pitch + x * 4);
                *pixel = *(uint32_t*)(buffer + y * vinfo.pitch + x * 4);
            }
        }

        display_tga(cursor, &vinfo, xpos, ypos, UNBUFFERED);
    }

    cleanup();

    return 0;
}
