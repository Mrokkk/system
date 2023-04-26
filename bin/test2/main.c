#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

uint8_t* framebuffer_data;

static inline uint32_t color_make(uint8_t red, uint8_t green, uint8_t blue)
{
    return blue | (green << 8) | (red << 16);
}

static inline void pixel_set(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t* pixel = (uint32_t*)(framebuffer_data + y * 0x1400 + x * 4);
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

int main(int argc, char* argv[])
{
    for (int i = 0; i < argc; ++i)
    {
        printf("argv[%u] = \"%s\"\n", i, argv[i]);
    }

    framebuffer_data = sbrk(720 * 0x1400);

    int fd = open("/dev/fb0", O_RDWR, 0);

    vector2_t position = {0, 0};
    vector2_t size = {1280, 720};
    draw_rectangle(position, size, 0xf5ce89);

    printf("framebuffer_data = 0x%x\n", framebuffer_data);

    write(fd, (void*)framebuffer_data, 720 * 0x1400);

    return 0;
}
