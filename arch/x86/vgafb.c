#define log_fmt(fmt) "vgafb: " fmt
#include <arch/bios.h>
#include <arch/earlycon.h>
#include <arch/real_mode.h>

#include <kernel/vga.h>
#include <kernel/kernel.h>
#include <kernel/api/ioctl.h>
#include <kernel/page_mmio.h>
#include <kernel/framebuffer.h>

// References:
// https://stanislavs.org/helppc/int_10.html

#define VGA_MODE_TEXT       0
#define VGA_MODE_GFX        1

#define DEFAULT_TEXT_MODE   3

static int vgafb_fb_mode_set(int resx, int resy, int bpp);

struct mode_info
{
    uint16_t resx, resy;
    uint8_t  mode;
    uint8_t  bits;
    uint8_t  type;
    uint8_t  color;
};

typedef struct mode_info mode_info_t;

// Standard VGA text modes; reference: http://www.columbia.edu/~em36/wpdos/videomodes.txt
static mode_info_t standard_modes[] = {
    {
        .mode = 0x0,
        .resx = 40,
        .resy = 25,
        .bits = 4,
        .type = VGA_MODE_TEXT,
        .color = false,
    },
    {
        .mode = 0x1,
        .resx = 40,
        .resy = 25,
        .bits = 4,
        .type = VGA_MODE_TEXT,
        .color = true,
    },
    {
        .mode = 0x2,
        .resx = 80,
        .resy = 25,
        .bits = 4,
        .type = VGA_MODE_TEXT,
        .color = true,
    },
    {
        .mode = 0x3,
        .resx = 80,
        .resy = 25,
        .bits = 4,
        .type = VGA_MODE_TEXT,
        .color = true,
    },
};

static mode_info_t* current_mode;

static fb_ops_t vgafb_ops = {
    .mode_set = &vgafb_fb_mode_set,
};

#define default_text_mode (&standard_modes[DEFAULT_TEXT_MODE])

#define BIOS_VIDEO_MODE_SET(regs, mode) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ah = 0; \
        regs.al = mode; \
        &regs; \
    })

#define BIOS_VIDEO_STATE_GET(regs) \
    ({ \
        memset(&regs, 0, sizeof(regs)); \
        regs.ah = 0xf; \
        &regs; \
    })

static void vgafb_framebuffer_setup(mode_info_t* mode)
{
    framebuffer.id       = "VGA FB";
    framebuffer.type     = FB_TYPE_TEXT;
    framebuffer.visual   = FB_VISUAL_PSEUDOCOLOR;
    framebuffer.type_aux = 0;
    framebuffer.pitch    = mode->resx * 2;
    framebuffer.width    = mode->resx;
    framebuffer.height   = mode->resy;
    framebuffer.bpp      = mode->bits;
    framebuffer.size     = framebuffer.pitch * framebuffer.height;
    framebuffer.accel    = FB_ACCEL_NONE;
    framebuffer.paddr    = 0xb8000;
    framebuffer.flags    = 0;
    framebuffer.ops = &vgafb_ops;
    if (!framebuffer.vaddr)
    {
        framebuffer.vaddr = mmio_map_wc(framebuffer.paddr, 64 * KiB, "fb");
    }
}

static int vgafb_fb_mode_set(int resx, int resy, int bpp)
{
    for (size_t i = 0; i < array_size(standard_modes); ++i)
    {
        mode_info_t* m = &standard_modes[i];
        if (m->resx == resx && m->resy == resy && m->bits == bpp)
        {
            regs_t regs;

            bios_call(BIOS_VIDEO, BIOS_VIDEO_MODE_SET(regs, m->mode));

            current_mode = m;

            vgafb_framebuffer_setup(current_mode);

            return 0;
        }
    }

    return -EINVAL;
}

int vgafb_initialize(void)
{
    regs_t regs;

    if (vga_probe())
    {
        log_info("not available");
        return -ENODEV;
    }

    log_notice("calling INT %#x AH=0xf", BIOS_VIDEO);

    bios_call(BIOS_VIDEO, BIOS_VIDEO_STATE_GET(regs));

    log_info("curent mode: %#x, columns: %u; current display page: %#x",
        regs.al, regs.ah, regs.bh);

    if (regs.al < array_size(standard_modes))
    {
        current_mode = &standard_modes[regs.al];
    }
    else
    {
        log_notice("unknown mode: %#x, setting default text mode", regs.al);
        bios_call(BIOS_VIDEO, BIOS_VIDEO_MODE_SET(regs, DEFAULT_TEXT_MODE));
        current_mode = &standard_modes[DEFAULT_TEXT_MODE];
    }

    earlycon_disable();
    vgafb_framebuffer_setup(current_mode);

    return 0;
}
