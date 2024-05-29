#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "input.h"
#include "macro.h"
#include "random.h"

#define GRID_SIZE 20

#define FLAGS_FOOD      1
#define FLAGS_SNAKE     2

#define FOREGROUND      0x43523d
#define BACKGROUND      0xc7f0d8
#define FOOD            0x40303d

#define CELL(x, y) (cells + (y) * xgrids + (x))

struct cell
{
    uint16_t x, y;
    uint8_t flags;
};

struct snake_block
{
    struct cell* cell;
    struct snake_block* next;
    struct snake_block* prev;
};

struct cell* cells = NULL;
struct snake_block* head;

size_t xgrids;
size_t ygrids;

char current_direction = 'd';

uint8_t* framebuffer;
uint8_t* buffer;
struct fb_var_screeninfo vinfo;

int input_fd;

static void snake_create(int segments);
static void graphic_mode_enter();
static void graphic_mode_exit();

void sighan()
{
    graphic_mode_exit();
    exit(EXIT_FAILURE);
}

static void signal_handlers_set(void)
{
    signal(SIGTERM, sighan);
    signal(SIGINT, sighan);
    signal(SIGTSTP, sighan);
    signal(SIGSEGV, sighan);
}

/*static void area_refresh(int x, int y, int w, int h)*/
/*{*/
    /*for (int i = x; i < x + w; ++i)*/
    /*{*/
        /*if (i >= (int)vinfo.xres)*/
        /*{*/
            /*break;*/
        /*}*/
        /*for (int j = y; j < y + h; ++j)*/
        /*{*/
            /*if (j >= (int)vinfo.yres)*/
            /*{*/
                /*break;*/
            /*}*/
            /*uint32_t* pixel = (uint32_t*)(framebuffer + j * vinfo.pitch + i * 4);*/
            /**pixel = *(uint32_t*)(buffer + j * vinfo.pitch + i * 4);*/
        /*}*/
    /*}*/
/*}*/

static void rectangle_draw(int x, int y, int w, int h, uint32_t color)
{
    for (int i = x; i < x + w; ++i)
    {
        if (i >= (int)vinfo.xres)
        {
            break;
        }
        for (int j = y; j < y + h; ++j)
        {
            if (j >= (int)vinfo.yres)
            {
                break;
            }
            uint32_t* pixel = (uint32_t*)(buffer + j * vinfo.pitch + i * 4);
            *pixel = color;
        }
    }
}

static void map_initialize()
{
    xgrids = vinfo.xres / GRID_SIZE;
    ygrids = vinfo.yres / GRID_SIZE;

    printf("Map: %lu x %lu\n", xgrids, ygrids);

    cells = malloc(sizeof(struct cell) * xgrids * ygrids);

    for (size_t i = 0; i < xgrids; ++i)
    {
        for (size_t j = 0; j < ygrids; ++j)
        {
            struct cell* cell = CELL(i, j);
            cell->flags = 0;
            cell->x = i;
            cell->y = j;
        }
    }

    rectangle_draw(0, 0, vinfo.xres, vinfo.yres, BACKGROUND);
}

static void graphic_mode_initialize()
{
    int fb_fd = open("/dev/fb0", O_RDWR, 0);

    if (fb_fd == -1)
    {
        die_perror("/dev/fb0");
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo))
    {
        die_perror("ioctl");
    }

    framebuffer = mmap(NULL, vinfo.yres * vinfo.pitch, PROT_READ | PROT_WRITE, MAP_PRIVATE, fb_fd, 0);

    if ((int)framebuffer == -1)
    {
        die_perror("mmap");
    }

    buffer = mmap(NULL, vinfo.yres * vinfo.pitch, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if ((int)buffer == -1)
    {
        die_perror("mmap buffer");
    }
}

static void graphic_mode_enter()
{
    if (ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS))
    {
        die_perror("ioctl");
    }
}

static void graphic_mode_exit()
{
    if (ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT))
    {
        perror("ioctl tty");
    }
}

static void initialize()
{
    signal_handlers_set();

    graphic_mode_initialize();
    input_initialize();
    map_initialize();
    snake_create(10);

    graphic_mode_enter();
}

static inline void cell_draw(struct cell* cell, int color)
{
    rectangle_draw(cell->x * GRID_SIZE, cell->y * GRID_SIZE, GRID_SIZE, GRID_SIZE, color);
}

static void snake_create(int segments)
{
    int x = xgrids / 2 + segments / 2;
    int y = ygrids / 2;

    head = malloc(sizeof(struct snake_block));
    head->cell = CELL(x--, y);
    head->cell->flags = FLAGS_SNAKE;

    struct snake_block* last = head;

    --segments;

    while (segments--)
    {
        struct snake_block* new_block = malloc(sizeof(struct snake_block));
        new_block->prev = last;
        new_block->cell = CELL(x--, y);
        new_block->cell->flags = FLAGS_SNAKE;
        last->next = new_block;
        last = new_block;
    }

    head->prev = last;
    last->next = head;

    struct snake_block* snake = head;
    while (snake->next != head)
    {
        cell_draw(snake->cell, FOREGROUND);
        snake = snake->next;
    }
}

static struct cell* cell_get_adjacent(struct cell* cell, int direction)
{
    size_t x = cell->x;
    size_t y = cell->y;

    switch (direction)
    {
        case 'w':
            if (y == 0)
            {
                return CELL(x, ygrids - 1);
            }
            return CELL(x, y - 1);
        case 's':
            if (y == ygrids - 1)
            {
                return CELL(x, 0);
            }
            return CELL(x, y + 1);
        case 'a':
            if (x == 0)
            {
                return CELL(xgrids - 1, y);
            }
            return CELL(x - 1, y);
        case 'd':
            if (x == xgrids - 1)
            {
                return CELL(0, y);
            }
            return CELL(x + 1, y);
        default: return NULL;
    }
}

static void snake_move()
{
    struct snake_block* new_head;
    struct snake_block* tail = head->prev;
    struct cell* old_tail = tail->cell;

    struct cell* next_cell = cell_get_adjacent(head->cell, current_direction);

    if (!next_cell || next_cell->flags & FLAGS_SNAKE)
    {
        graphic_mode_exit();
        printf("Game over!\n");
        exit(EXIT_FAILURE);
    }

    if (old_tail->flags & FLAGS_FOOD)
    {
        new_head = malloc(sizeof(struct snake_block));
        new_head->cell = next_cell;
        new_head->next = head;
        new_head->prev = tail;
        tail->next = new_head;
        head->prev = new_head;
        old_tail->flags = FLAGS_SNAKE;
        cell_draw(old_tail, FOREGROUND);
    }
    else
    {
        new_head = tail;
        old_tail->flags = 0;
        tail->cell = next_cell;
        cell_draw(old_tail, BACKGROUND);
    }

    next_cell->flags |= FLAGS_SNAKE;

    cell_draw(
        new_head->cell,
        new_head->cell->flags & FLAGS_FOOD
            ? FOOD
            : FOREGROUND);

    head = new_head;
}

#define CHECK_INPUT(normal, opposite) \
    case normal: \
        if (current_direction != opposite) \
        { \
            current_direction = *c; \
        } \
        break

static void snake_direction_set(char* c)
{
    if (!c)
    {
        return;
    }

    switch (*c)
    {
        CHECK_INPUT('w', 's');
        CHECK_INPUT('s', 'w');
        CHECK_INPUT('a', 'd');
        CHECK_INPUT('d', 'a');
    }
}

int main()
{
    initialize();

    int i = 0;
    srand(time(NULL));
    while (1)
    {
        snake_direction_set(input_read());

        if (i % 500000 == 0)
        {
            snake_move();

            if (i % 8000000 == 0)
            {
                struct cell* food_cell;
                do
                {
                    food_cell = CELL(random() % xgrids, random() % ygrids);
                }
                while (food_cell->flags & FLAGS_SNAKE);

                food_cell->flags = FLAGS_FOOD;

                cell_draw(food_cell, FOOD);
            }

            memcpy(framebuffer, buffer, vinfo.yres * vinfo.pitch);
        }

        ++i;
    }

    return 0;
}
