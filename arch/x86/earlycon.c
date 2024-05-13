#define log_fmt(fmt) "earlycon: " fmt
#include <arch/io.h>
#include <arch/bios.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

#define VIDEOMEM            ((uint16_t*)0xb8000)
#define MODE                0x03
#define RESX                80
#define RESY                25
#define FORECOLOR(x)        (x)
#define BACKCOLOR(x)        ((x << 4) & 0x7f)
#define ATTR                (FORECOLOR(7) | BACKCOLOR(0))

#define VIDEO_MODE_GET(regs) \
    ({ \
        regs.ax = 0x0f00; \
        &regs; \
    })

#define VIDEO_MODE_SET(regs) \
    ({ \
        regs.ax = 0x0000 | MODE; \
        &regs; \
    })

#define VIDEO_SCROLL_UP(regs) \
    ({ \
        regs.ax = 0x0601; \
        regs.bh = ATTR; \
        regs.cx = 0x0000; \
        regs.dh = RESY; \
        regs.dl = RESX; \
        &regs; \
    })

#define KBD_READ(regs) \
    ({ \
        regs.ax = 0x0000; \
        &regs; \
    })

static void cls(void);
static void csr_move(uint16_t off);
static void earlycon_print(const char* s);
static void earlycon_char_print(size_t row, size_t col, char c);

static uint8_t curx, cury;
static bool disabled;

UNMAP_AFTER_INIT int earlycon_init(void)
{
    regs_t regs;

    bios_call(BIOS_VIDEO, VIDEO_MODE_GET(regs));

    log_info("current mode: %x", regs.al, regs.ah);

    if (regs.al != MODE)
    {
        log_info("setting mode %02x", MODE);
        bios_call(BIOS_VIDEO, VIDEO_MODE_SET(regs));
    }

    cls();
    csr_move(0);

    printk_early_register(&earlycon_print);

    return 0;
}

UNMAP_AFTER_INIT void earlycon_disable(void)
{
    printk_early_register(NULL);
    disabled = true;
}

void earlycon_enable(void)
{
    regs_t regs;

    if (disabled)
    {
        curx = cury = 0;
        csr_move(0);
        bios_call(BIOS_VIDEO, VIDEO_MODE_SET(regs));
    }

    printk_early_register(&earlycon_print);
}

static inline void videomem_write(uint16_t data, uint16_t offset)
{
    writew(data | (ATTR << 8), VIDEOMEM + offset);
}

static void csr_move(uint16_t off)
{
    outb(14, 0x3d4);
    outb(off >> 8, 0x3d5);
    outb(15, 0x3d4);
    outb(off, 0x3d5);
}

static void cls(void)
{
    memsetw(VIDEOMEM, 0x720, RESX * RESY);
    csr_move(0);
}

static inline void earlycon_char_print(size_t row, size_t col, char c)
{
    uint16_t off = row * RESX + col;
    videomem_write(c, off);
    csr_move(off + 1);
}

char earlycon_read(void)
{
    regs_t regs;
    bios_call(BIOS_KEYBOARD, KBD_READ(regs));
    return regs.al;
}

static void earlycon_print(const char* s)
{
    regs_t regs;
    bool escape_seq = false;
    for (; *s; ++s)
    {
        if (cury >= RESY)
        {
            bios_call(BIOS_VIDEO, VIDEO_SCROLL_UP(regs));
            --cury;
            curx = 0;
        }

        if (*s == '\n')
        {
            cury++;
            curx = 0;
            continue;
        }
        else if (escape_seq)
        {
            if (*s == 'm')
            {
                escape_seq = false;
            }
            continue;
        }
        else if (*s == '\e')
        {
            escape_seq = true;
            continue;
        }
        else if (*s < 0x20)
        {
            continue;
        }

        earlycon_char_print(cury, curx++, *s);

        if (curx >= RESX)
        {
            curx = 0;
            ++cury;
        }
    }
}
