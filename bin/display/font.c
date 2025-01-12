#include "font.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define _STDINT_H
#define SSFN_IMPLEMENTATION
#include <ssfn.h>

#include "utils.h"
#include "definitions.h"

static ssfn_t ctx;
static ssfn_buf_t buf;

#define HEIGHT (WINDOW_BAR_HEIGHT - 4)

ssfn_font_t* font;

int font_load(void* framebuffer)
{
    int err;
    void* data = file_map(FONT_PATH);

    if (!data)
    {
        perror(FONT_PATH);
        return EXIT_FAILURE;
    }

    font = data;
    buf.ptr = framebuffer;
    buf.w = vinfo.xres;
    buf.h = vinfo.yres;
    buf.p = vinfo.pitch;

    if ((err = ssfn_load(&ctx, data)) != SSFN_OK)
    {
        fprintf(stderr, FONT_PATH ": cannot load in ssfn: %d", err);
        return EXIT_FAILURE;
    }

    if ((err = ssfn_select(&ctx, SSFN_FAMILY_ANY, NULL, SSFN_STYLE_REGULAR, HEIGHT)) < 0)
    {
        fprintf(stderr, FONT_PATH ": cannot select in ssfn: %d", err);
        return EXIT_FAILURE;
    }

    return 0;
}

void string_to_framebuffer(int x, int y, char* string, uint32_t fgcolor, uint32_t bgcolor)
{
    buf.x = x;
    buf.y = y + HEIGHT - 4; // FIXME: why ssf cannot draw font starting from 0?
    buf.fg = fgcolor | 0xff000000;
    buf.bg = bgcolor | 0xff000000;

    for (; *string; string++)
    {
        ssfn_render(&ctx, &buf, string);
    }
}
