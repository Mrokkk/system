#define log_fmt(fmt) "earlycon: " fmt
#include <arch/io.h>
#include <arch/bios.h>
#include <arch/earlycon.h>

#include <kernel/fs.h>
#include <kernel/dev.h>
#include <kernel/tty.h>
#include <kernel/init.h>
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
static void csr_move(void);

static int earlycon_open(tty_t* tty, file_t* file);
static int earlycon_close(tty_t* tty, file_t* file);
static int earlycon_write(tty_t* tty, const char* buffer, size_t size);
static void earlycon_putch(tty_t* tty, int c);

static bool escape_seq;
static uint16_t curx;
static uint16_t cury;
static unsigned line_nr;

static tty_driver_t tty_driver = {
    .name = "earlycon",
    .major = MAJOR_CHR_EARLYCON,
    .minor_start = 0,
    .num = 1,
    .driver_data = NULL,
    .open = &earlycon_open,
    .close = &earlycon_close,
    .write = &earlycon_write,
    .putch = &earlycon_putch,
};

UNMAP_AFTER_INIT void earlycon_init(void)
{
    tty_driver_register(&tty_driver);
    param_call_if_set(KERNEL_PARAM("earlycon"), &earlycon_enable);
}

UNMAP_AFTER_INIT void earlycon_disable(void)
{
    tty_driver.initialized = false;
}

int earlycon_enable(void)
{
    regs_t regs;

    if (!tty_driver.initialized)
    {
        bios_call(BIOS_VIDEO, VIDEO_MODE_GET(regs));

        log_info("current mode: %x", regs.al, regs.ah);

        if (regs.al != MODE)
        {
            log_info("setting mode %02x", MODE);
            bios_call(BIOS_VIDEO, VIDEO_MODE_SET(regs));
        }

        line_nr = 0;
        tty_driver.initialized = true;
        cls();
    }

    return 0;
}

static void csr_move(void)
{
    uint16_t off = cury * RESX + curx;
    outb(14, 0x3d4);
    outb(off >> 8, 0x3d5);
    outb(15, 0x3d4);
    outb(off, 0x3d5);
}

static void cls(void)
{
    curx = cury = 0;
    memsetw(VIDEOMEM, 0x720, RESX * RESY);
    csr_move();
}

static inline void earlycon_char_print(size_t row, size_t col, char c)
{
    uint16_t off = row * RESX + col;
    writew(c | (ATTR << 8), VIDEOMEM + off);
}

char earlycon_read(void)
{
    regs_t regs;
    bios_call(BIOS_KEYBOARD, KBD_READ(regs));
    return regs.al;
}

static int earlycon_open(tty_t*, file_t*)
{
    return 0;
}

static int earlycon_close(tty_t*, file_t*)
{
    return 0;
}

static void earlycon_putch_impl(int c)
{
    regs_t regs;

    if (cury >= RESY)
    {
        bios_call(BIOS_VIDEO, VIDEO_SCROLL_UP(regs));
        --cury;
        curx = 0;
    }

    if (c == '\n')
    {
        cury++;
        line_nr++;
        curx = 0;
        return;
    }
    else if (escape_seq)
    {
        if (c == 'm')
        {
            escape_seq = false;
        }
        return;
    }
    else if (c == '\e')
    {
        escape_seq = true;
        return;
    }
    else if (c < 0x20)
    {
        return;
    }

    earlycon_char_print(cury, curx++, c);

    if (curx >= RESX)
    {
        curx = 0;
        ++cury;
    }
}

static int earlycon_write(tty_t* tty, const char* buffer, size_t size)
{
    size_t old = size;

    if (unlikely(!tty_driver.initialized))
    {
        return 0;
    }

    while (size--)
    {
        earlycon_putch(tty, *buffer++);
    }

    csr_move();

    return old;
}

static void earlycon_putch(tty_t*, int c)
{
    if (unlikely(!tty_driver.initialized))
    {
        return;
    }

    earlycon_putch_impl(c);
    csr_move();
}
