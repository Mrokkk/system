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

#include "font.h"
#include "targa.h"
#include "utils.h"
#include "object.h"
#include "window.h"
#include "definitions.h"

#define min(a, b) a < b ? a : b;

#define MOUSE_LEFT_BUTTON       1
#define MOUSE_RIGHT_BUTTON      2
#define MOUSE_MIDDLE_BUTTON     4
#define MOUSE_BUTTONS_MASK      7

uint8_t* framebuffer;
uint8_t* buffer;
struct tga_header* cursor;
struct tga_header* close_icon;
struct tga_header* close_pressed_icon;
struct fb_var_screeninfo vinfo;
static int xpos = 0, ypos = 0;
static int old_xpos= 0, old_ypos = 0;
char button_prev;
int mousefd;
window_t* main_window;

#define BUFFERED    1
#define UNBUFFERED  2

static void cleanup()
{
    if (ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT))
    {
        perror("ioctl tty");
    }
}

static void sighan()
{
    cleanup();
    exit(EXIT_FAILURE);
}

static void signal_handlers_set(void)
{
    signal(SIGTERM, sighan);
    signal(SIGINT, sighan);
    signal(SIGTSTP, sighan);
    signal(SIGSEGV, sighan);
}

static void* alloc(size_t size)
{
    void* ptr = malloc(size);

    if (UNLIKELY(!ptr))
    {
        fprintf(stderr, "display: cannot allocate %lu bytes\n", size);
        sighan();
    }

    return ptr;
}

static void area_refresh(int x, int y, int w, int h)
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
            uint32_t* pixel = (uint32_t*)(framebuffer + j * vinfo.pitch + i * 4);
            *pixel = *(uint32_t*)(buffer + j * vinfo.pitch + i * 4);
        }
    }
}

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

static void cursor_position_update(int xmov, int ymov)
{
    old_xpos = xpos;
    old_ypos = ypos;

    xpos += xmov;
    ypos -= ymov;

    xpos = clamp(xpos, 0, vinfo.xres);
    ypos = clamp(ypos, 0, vinfo.yres);
}

static void cursor_draw(void)
{
    area_refresh(old_xpos, old_ypos, cursor->w, cursor->h);
    tga_to_framebuffer(cursor, xpos, ypos, &vinfo, framebuffer);
}

int initialize()
{
    int fb_fd;

    signal_handlers_set();

    mousefd = open("/dev/mouse", O_RDONLY, 0);

    if (mousefd == -1)
    {
        perror("/dev/mouse");
        return EXIT_FAILURE;
    }

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

    buffer = mmap(NULL, vinfo.yres * vinfo.pitch, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if ((int)buffer == -1)
    {
        perror("mmap buffer");
        return EXIT_FAILURE;
    }

    if (!(cursor = file_map("/cursor.tga")) ||
        !(close_icon = file_map("/close.tga")) ||
        !(close_pressed_icon = file_map("/close_pressed.tga")))
    {
        return EXIT_FAILURE;
    }

    window_t* window = alloc(sizeof(window_t));
    window->position.x = 0;
    window->position.y = 0;
    window->size.x = vinfo.xres;
    window->size.y = vinfo.yres;
    list_init(&window->objects);
    list_init(&window->dirty);
    main_window_set(window);

    font_load();

    if (ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS))
    {
        perror("ioctl tty");
        return EXIT_FAILURE;
    }

    return 0;
}

#define button_just_pressed() \
    (!(button_prev & MOUSE_LEFT_BUTTON) && (button_cur & MOUSE_LEFT_BUTTON))

#define button_just_released() \
    ((button_prev & MOUSE_LEFT_BUTTON) && !(button_cur & MOUSE_LEFT_BUTTON))

void start(void)
{
    char button_cur;
    char buf[3];
    object_t* o;
    list_head_t* node;
    list_head_t* prev;
    bool button_just_pressed, button_just_released;

    xpos = old_xpos = vinfo.xres / 2;
    ypos = old_ypos = vinfo.yres / 2;

    memset(buffer, WINDOW_BODY_COLOR, vinfo.yres * vinfo.pitch);

    rectangle_draw(
        WINDOW_FRAME_WIDTH,
        WINDOW_FRAME_WIDTH,
        vinfo.xres - 2 * WINDOW_FRAME_WIDTH,
        WINDOW_BAR_HEIGHT,
        WINDOW_BAR_COLOR);

    string_to_framebuffer(
        WINDOW_FRAME_WIDTH * 3,
        WINDOW_FRAME_WIDTH * 3,
        main_window->title,
        0xffffff,
        WINDOW_BAR_COLOR,
        buffer);

    list_for_each_entry(o, &main_window->objects, list)
    {
        object_draw(o, false);
        list_del(&o->dirty);
        area_refresh(o->position.x, o->position.y, o->size.x, o->size.y);
    }

    memcpy(framebuffer, buffer, vinfo.yres * vinfo.pitch);
    cursor_draw();

    while (1)
    {
        read(mousefd, buf, 3);

        button_cur = buf[0] & MOUSE_BUTTONS_MASK;
        button_just_pressed = button_just_pressed();
        button_just_released = button_just_released();
        button_prev = button_cur;

        cursor_position_update(buf[1], buf[2]);

        list_for_each_entry(o, &main_window->objects, list)
        {
            if ((button_just_pressed || button_just_released) &&
                xpos >= o->position.x &&
                ypos >= o->position.y &&
                xpos <= o->position.x + o->size.x &&
                ypos <= o->position.y + o->size.y)
            {
                if (button_just_pressed && o->pressed_img)
                {
                    object_draw(o, button_cur & MOUSE_LEFT_BUTTON);
                }

                if (button_just_released && o->on_click)
                {
                    vector2_t pos = {.x = xpos, .y = ypos};
                    o->on_click(o, &pos);
                }
            }
            else if (button_just_released)
            {
                object_draw(o, false);
            }
        }

        for (node = main_window->dirty.next; node != &main_window->dirty;)
        {
            prev = node;
            o = list_entry(node, object_t, dirty);
            area_refresh(o->position.x, o->position.y, o->size.x, o->size.y);
            node = node->next;
            list_del(prev);
        }

        cursor_draw();
    }
}

int main(int argc, char* argv[])
{
    const char* img_path;
    struct tga_header* img;

    if (argc < 2)
    {
        fprintf(stderr, "display: no image path given\n");
        return EXIT_FAILURE;
    }

    img_path = argv[1];

    if (initialize())
    {
        return EXIT_FAILURE;
    }

    if (!(img = file_map(img_path)))
    {
        return EXIT_FAILURE;
    }

    object_t* close_button = alloc(sizeof(*close_button));
    close_button->normal_img = close_icon;
    close_button->pressed_img = close_pressed_icon;
    close_button->position.x = vinfo.xres - close_icon->w - 2 * WINDOW_FRAME_WIDTH;
    close_button->position.y = 2 * WINDOW_FRAME_WIDTH;
    close_button->size.x = close_icon->w;
    close_button->size.y = close_icon->h;
    close_button->on_click = &sighan;
    list_init(&close_button->list);
    list_init(&close_button->dirty);

    object_t* image = alloc(sizeof(*image));
    image->normal_img = img;
    image->pressed_img = NULL;
    image->position.x = (vinfo.xres - img->w) / 2;
    image->position.y = (vinfo.yres - img->h) / 2;
    image->size.x = img->w;
    image->size.y = img->h;
    image->on_click = NULL;
    list_init(&image->list);
    list_init(&image->dirty);

    window_object_add(main_window, image);
    window_object_add(main_window, close_button);
    main_window->title = argv[0];

    start();

    return 0;
}
