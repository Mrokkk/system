#include "fbcon.h"

#include "tty.h"
#include "font.h"
#include "console.h"
#include "framebuffer.h"

enum palette
{
    COLOR_BLACK         = 0x000000,
    COLOR_RED           = 0xcd0000,
    COLOR_GREEN         = 0x00cd00,
    COLOR_YELLOW        = 0xcdcd00,
    COLOR_BLUE          = 0x006fb8,
    COLOR_MAGENTA       = 0xcd00cd,
    COLOR_CYAN          = 0x00cdcd,
    COLOR_WHITE         = 0x999999,
    COLOR_GRAY          = 0x7f7f7f,
    COLOR_BRIGHTRED     = 0xff0000,
    COLOR_BRIGHTGREEN   = 0x00ff00,
    COLOR_BRIGHTYELLOW  = 0xffff00,
    COLOR_BRIGHTBLUE    = 0x0000ff,
    COLOR_BRIGHTMAGENTA = 0xff00ff,
    COLOR_BRIGHTCYAN    = 0x00ffff,
    COLOR_BRIGHTWHITE   = 0xffffff,
};

typedef enum
{
    NORMAL,
    COLOR256,
    RGB,
} color_mode_t;

typedef struct
{
    uint8_t* fb;
    uint32_t pitch;
    uint32_t font_height_offset;
    uint32_t font_width_offset;
    uint32_t fg_color;
    uint32_t bg_color;
} data_t;

data_t data;

int fbcon_init(console_driver_t* driver, size_t* resy, size_t* resx)
{
    driver->data = &data;
    data.fb = framebuffer.fb;
    data.pitch = framebuffer.pitch;
    data.font_height_offset = framebuffer.pitch * font.height;
    data.font_width_offset =  (framebuffer.bpp / 8) * font.width;
    data.fg_color = COLOR_WHITE;
    data.bg_color = COLOR_BLACK;
    *resy = framebuffer.height / font.height;
    *resx = framebuffer.width / font.width - 1; // FIXME: it crashes
    return 0;
}

void fbcon_char_print(console_driver_t*, size_t y, size_t x, unsigned int c)
{
    uint32_t line, mask;
    uint8_t* glyph = font.glyphs[c].bytes;
    uint32_t offset = data.font_height_offset * y + data.font_width_offset * x;

    for (uint32_t y = 0; y < font.bytes_per_glyph; y++, ++glyph, offset += data.pitch)
    {
        line = offset;
        mask = 1 << 7;
        for (uint32_t x = 0; x < font.bytes_per_glyph; x++)
        {
            uint32_t value = *glyph & mask ? data.fg_color : data.bg_color;
            uint32_t* pixel = (uint32_t*)(data.fb + line);
            *pixel = value;
            mask >>= 1;
            line += 4;
        }
    }
}

void fbcon_setsgr(console_driver_t*, uint32_t params[], size_t count)
{
    color_mode_t mode = NORMAL;
    uint32_t color = 0;
    for (size_t i = 0; i < count; ++i)
    {
        uint32_t param = params[i];
        log_debug(DEBUG_CONSOLE, "param %d = %d", i, param);

        if (mode == RGB)
        {
            log_debug(DEBUG_CONSOLE, "color = %d", param);
            color = (color << 8) | param;
            continue;
        }

        switch (param)
        {
            case 0:
                data.fg_color = COLOR_WHITE;
                data.bg_color = COLOR_BLACK;
                break;
            case 30: data.fg_color = COLOR_BLACK; break;
            case 31: data.fg_color = COLOR_RED; break;
            case 32: data.fg_color = COLOR_GREEN; break;
            case 33: data.fg_color = COLOR_YELLOW; break;
            case 34: data.fg_color = COLOR_BLUE; break;
            case 35: data.fg_color = COLOR_MAGENTA; break;
            case 36: data.fg_color = COLOR_CYAN; break;
            case 38:
                switch (params[++i])
                {
                    case 2:
                        mode = RGB;
                    case 5:
                        // TODO:
                }
                break;
            case 40: data.bg_color = COLOR_BLACK; break;
            case 41: data.bg_color = COLOR_RED; break;
            case 42: data.bg_color = COLOR_GREEN; break;
            case 43: data.bg_color = COLOR_YELLOW; break;
            case 44: data.bg_color = COLOR_BLUE; break;
            case 45: data.bg_color = COLOR_MAGENTA; break;
            case 46: data.bg_color = COLOR_CYAN; break;
            case 90: data.fg_color = COLOR_GRAY; break;
            case 91: data.fg_color = COLOR_BRIGHTRED; break;
            case 92: data.fg_color = COLOR_BRIGHTGREEN; break;
            case 93: data.fg_color = COLOR_BRIGHTYELLOW; break;
            case 94: data.fg_color = COLOR_BRIGHTBLUE; break;
            case 95: data.fg_color = COLOR_BRIGHTMAGENTA; break;
            case 96: data.fg_color = COLOR_BRIGHTCYAN; break;
            case 97: data.fg_color = COLOR_BRIGHTWHITE; break;
        }
    }
    if (color)
    {
        data.fg_color = color;
    }
}
