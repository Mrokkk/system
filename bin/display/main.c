#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define addr(x) ((uint32_t)(x))
#define ptr(x)  ((void*)(x))

#define RESX 1600
#define RESY 900
#define PITCH RESX * 4

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
} __attribute__((packed));

uint8_t* framebuffer;
uint8_t* buffer;

static inline uint32_t color_make(uint8_t red, uint8_t green, uint8_t blue)
{
    return blue | (green << 8) | (red << 16);
}

static inline void pixel_set(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t* pixel = (uint32_t*)(buffer + y * PITCH + x * 4);
    *pixel = color;
}

typedef struct vector2
{
    uint16_t x;
    uint16_t y;
} vector2_t;

void draw_rectangle(vector2_t position, vector2_t size, uint32_t color)
{
    for (uint32_t i = 0; i < size.x; ++i)
    {
        for (uint32_t j = 0; j < size.y; ++j)
        {
            pixel_set(i + position.x, j + position.y, color);
        }
    }
}

static void img_draw(int x, int y, int width, int height, uint32_t* img)
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

int display_tga(struct tga_header *base)
{
    uint32_t* img;

    if (base->magic1 != 0 || base->colormap != 0 ||
        base->encoding != 2 || base->cmaporig != 0 ||
        base->cmapent != 0 || base->x != 0 ||
        base->bpp != 32 || base->pixeltype != 40)
    {
        printf("unsupported format!\n");
        return 1;
    }

    img = ptr(addr(base) + sizeof(struct tga_header));
    img_draw(0, 0, base->w, base->h, img);
    memcpy(framebuffer, buffer, RESY * PITCH);

    return 0;
}

int main(int argc, char* argv[])
{
    struct stat s;
    int fb_fd, img_fd;
    const char* img_path;
    void* img;

    if (argc < 2)
    {
        printf("display: no image path given\n");
        return EXIT_FAILURE;
    }

    img_path = argv[1];

    fb_fd = open("/dev/fb0", O_RDWR, 0);

    if (fb_fd < 0)
    {
        perror("/dev/fb0");
        return EXIT_FAILURE;
    }

    framebuffer = mmap(NULL, RESY * PITCH, PROT_READ | PROT_WRITE, MAP_PRIVATE, fb_fd, 0);
    if ((int)framebuffer == -1)
    {
        perror("mmap");
        return EXIT_FAILURE;
    }

    stat(img_path, &s);
    img_fd = open(img_path, O_RDONLY, 0);

    if (img_fd < 0)
    {
        perror(img_path);
        return EXIT_FAILURE;
    }

    img = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, img_fd, 0);

    if ((int)img == -1)
    {
        perror("mmap img");
        return EXIT_FAILURE;
    }

    buffer = mmap(NULL, RESY * PITCH, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if ((int)buffer == -1)
    {
        perror("mmap buffer");
        return EXIT_FAILURE;
    }

    memset(buffer, 0, RESY * PITCH);

    if (display_tga(img))
    {
        return EXIT_FAILURE;
    }

    char buf[2];
    read(STDIN_FILENO, buf, 1);

    return 0;
}
