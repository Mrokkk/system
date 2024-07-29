#define log_fmt(fmt) "vbe: " fmt
#include <arch/vbe.h>
#include <arch/bios.h>
#include <arch/i8259.h>
#include <arch/earlycon.h>
#include <arch/multiboot.h>
#include <kernel/init.h>
#include <kernel/page.h>
#include <kernel/kernel.h>

#define VBE_INFO_BLOCK_ADDR         0x1000
#define VBE_MODE_INFO_BLOCK_ADDR    0x1400
#define VBE_EDID_ADDR               0x1400

#define DEFAULT_TEXT_MODE   3
#define BITS                32

#define VBE_MODE_GET(regs) \
    ({ \
        regs.ax = VBE_GET_INFO_BLOCK; \
        regs.di = VBE_INFO_BLOCK_ADDR; \
        &regs; \
    })

#define VBE_EDID_GET(regs) \
    ({ \
        regs.ax = 0x4f15; \
        regs.bl = 0x01; \
        regs.cx = 0x00; \
        regs.dx = 0x00; \
        regs.di = VBE_EDID_ADDR; \
        &regs; \
    })

static mode_info_t* desired_mode;
static mode_info_t* current_mode;
static mode_info_t* modes;
struct framebuffer* framebuffer_ptr;

struct
{
    uint16_t resx, resy, bits;
} res_priorities[] = {
    {1920, 1080, 32},
    {1600, 900, 32},
    {1400, 1050, 32},
    {1440, 900, 32},
    {1280, 800, 32},
    {1024, 768, 32},
    {800, 600, 32},
    {1024, 768, 24},
    {800, 600, 24},
};

// Standard VGA modes; reference: http://www.columbia.edu/~em36/wpdos/videomodes.txt
static mode_info_t standard_modes[] = {
    {
        .mode = 0x0,
        .resx = 40,
        .resy = 25,
        .pitch = 40 * 2,
        .bits = 4,
        .type = VBE_MODE_TEXT,
        .color_support = true,
    },
    {
        .mode = 0x1,
        .resx = 40,
        .resy = 25,
        .pitch = 40 * 2,
        .bits = 4,
        .type = VBE_MODE_TEXT,
        .color_support = true,
    },
    {
        .mode = 0x2,
        .resx = 80,
        .resy = 25,
        .pitch = 80 * 2,
        .bits = 4,
        .type = VBE_MODE_TEXT,
        .color_support = true,
    },
    {
        .mode = 0x3,
        .resx = 80,
        .resy = 25,
        .pitch = 80 * 2,
        .bits = 4,
        .type = VBE_MODE_TEXT,
        .color_support = true,
    },
};

#define default_text_mode (&standard_modes[DEFAULT_TEXT_MODE])

static char* video_mode_print(char* buf, mode_info_t* m)
{
    int i;
    i = sprintf(buf, "%x: %ux%ux%u ", m->mode, m->resx, m->resy, m->bits);
    i += sprintf(buf + i, "%s %s",
        m->type == VBE_MODE_GRAPHICS ? "graphics" : "text",
        m->color_support ? "color" : "mono");

    if (m->framebuffer)
    {
        sprintf(buf + i, " fb=%x", m->framebuffer);
    }

    return buf;
}

static inline int video_mode_set(mode_info_t* mode)
{
    char buf[48];
    regs_t regs;

    log_notice("setting mode %s", video_mode_print(buf, mode));

    regs.ax = VBE_SET_MODE;
    regs.bx = mode->mode | (mode->framebuffer ? VBE_MODE_USE_LINEAR_FB : 0);
    bios_call(BIOS_VIDEO, &regs);

    if (regs.ax != VBE_SUPPORTED)
    {
        log_warning("setting mode failed");
        return -EIO;
    }

    current_mode = mode;

    return 0;
}

static inline uint16_t vbe_mode_read(void)
{
    regs_t regs;

    regs.ax = VBE_GET_MODE;
    bios_call(BIOS_VIDEO, &regs);

    if (regs.ax != VBE_SUPPORTED)
    {
        log_warning("cannot read mode");
        return 0;
    }

    return regs.bx & 0x7fff;
}

static int video_modes_print(void)
{
    mode_info_t* v;
    char buf[64];

    for (v = modes; v->is_valid; ++v)
    {
        log_notice("%s", video_mode_print(buf, v));
    }
    return 0;
}

void max_resolution_read(int* x, int* y)
{
    uint8_t* edid = ptr(VBE_EDID_ADDR);
    *x = edid[0x38] | ((int)(edid[0x3a] & 0xf0) << 4);
    *y = edid[0x3b] | ((int)(edid[0x3d] & 0xf0) << 4);
}

static void mode_info_fill(
    mode_info_t* mode,
    uint16_t vbe_mode,
    vbe_mib_t* vbe_mode_info)
{
    mode->resx = vbe_mode_info->resx;
    mode->resy = vbe_mode_info->resy;
    mode->pitch = vbe_mode_info->pitch;
    mode->mode = vbe_mode;
    mode->bits = vbe_mode_info->bits_per_pixel;
    mode->type = VBE_MODE_TYPE(vbe_mode_info->mode_attr);
    mode->color_support = VBE_MODE_COLOR(vbe_mode_info->mode_attr);
    mode->framebuffer = vbe_mode_info->framebuffer;
}

static mode_info_t* mode_find(void)
{
    size_t prio = ~0ul;
    mode_info_t* desired = NULL;

    for (mode_info_t* m = modes; m->is_valid; ++m)
    {
        for (size_t i = 0; i < array_size(res_priorities); ++i)
        {
            if (i < prio &&
                m->resx == res_priorities[i].resx &&
                m->resy == res_priorities[i].resy &&
                m->bits == res_priorities[i].bits &&
                m->framebuffer)
            {
                prio = i;
                desired = m;
            }
        }
    }

    return desired;
}

int vbe_initialize(void)
{
    int resx, resy;
    size_t index = 0;
    regs_t regs;
    vbe_info_block_t* vbe = ptr(VBE_INFO_BLOCK_ADDR);
    vbe_mib_t* mode_info = ptr(VBE_MODE_INFO_BLOCK_ADDR);

    // IBM ThinkPad T42: apparently, BIOS calls disable couple of IRQs in i8259,
    // (e.g. RTC and PIT) so it's better to disable it here and reenable before returning
    if (i8259_used)
    {
        i8259_disable();
    }

    bios_call(BIOS_VIDEO, VBE_MODE_GET(regs));

    if (regs.ax != VBE_SUPPORTED)
    {
        log_info("VBE_GET_MODE_INFO failed with %x", regs.ah);
        return -ENOSYS;
    }

    if (vbe->version < 0x200)
    {
        log_warning("too old version: %x", vbe->version);
        goto setup_fb;
    }

    log_notice("product name: %s; version: %x", farptr(vbe->oem_product_name), vbe->version);

    modes = single_page();

    if (unlikely(!modes))
    {
        return -ENOMEM;
    }

    memset(modes, 0, PAGE_SIZE);

    bios_call(BIOS_VIDEO, VBE_EDID_GET(regs));

    resx = resy = 0;

    if (regs.al != 0x4f || regs.ah == 1 || *(uint64_t*)VBE_EDID_ADDR != EDID_SIGNATURE)
    {
        log_warning("EDID reading unsupported/failed; al=%x ah=%x sig=%llx",
            regs.al,
            regs.ah,
            *(uint64_t*)VBE_EDID_ADDR);
    }
    else
    {
        max_resolution_read(&resx, &resy);
        log_info("edid: %ux%u", resx, resy);
    }

    uint16_t current_vbe_mode = vbe_mode_read();

    for (uint16_t* mode_ptr = farptr(vbe->video_mode); *mode_ptr != VBE_MODE_END; ++mode_ptr)
    {
        mode_info_t* mode;
        regs.ax = VBE_GET_MODE_INFO;
        regs.cx = *mode_ptr;
        regs.di = VBE_MODE_INFO_BLOCK_ADDR;
        bios_call(BIOS_VIDEO, &regs);

        if (regs.ax != VBE_SUPPORTED)
        {
            continue;
        }

        mode = &modes[index++];
        mode_info_fill(mode, *mode_ptr, mode_info);

        if (mode->mode == current_vbe_mode)
        {
            current_mode = mode;
        }

        if (!desired_mode &&
            resx &&
            mode->resx == resx &&
            mode->resy == resy &&
            mode->bits == BITS)
        {
            desired_mode = mode;
        }
    }

    log_notice("supported modes: %u", index);

    param_call_if_set(KERNEL_PARAM("vbeprint"), &video_modes_print);

    if (current_mode)
    {
        char buf[48];
        log_notice("current: %s", video_mode_print(buf, current_mode));
    }

setup_fb:
    earlycon_disable();

    desired_mode = desired_mode ? : mode_find();

    if (desired_mode)
    {
        video_mode_set(desired_mode);
    }

    if (!framebuffer_ptr)
    {
        framebuffer_ptr = alloc(struct framebuffer);
    }

    if (unlikely(!current_mode))
    {
        log_notice("no current mode; reading it by INT %x AH=0xf", BIOS_VIDEO);
        regs.ah = 0xf;
        bios_call(BIOS_VIDEO, &regs);

        if (regs.al < 4)
        {
            current_mode = &standard_modes[regs.al];
        }
        else
        {
            log_notice("unknown mode: %x, setting default text mode", regs.al);
            video_mode_set(default_text_mode);
        }
    }

    if (current_mode->type == VBE_MODE_GRAPHICS && current_mode->framebuffer)
    {
        framebuffer_ptr->addr = current_mode->framebuffer;
        framebuffer_ptr->type = MULTIBOOT_FRAMEBUFFER_TYPE_RGB;
    }
    else if (current_mode->type == VBE_MODE_TEXT)
    {
        framebuffer_ptr->addr = 0xb8000;
        framebuffer_ptr->type = MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT;
    }
    else
    {
        log_warning("incompatible mode: %x type %x fb %x",
            current_mode->mode,
            current_mode->type,
            current_mode->framebuffer);
        return -EINVAL;
    }

    framebuffer_ptr->pitch = current_mode->pitch;
    framebuffer_ptr->width = current_mode->resx;
    framebuffer_ptr->height = current_mode->resy;
    framebuffer_ptr->bpp = current_mode->bits;

    if (i8259_used)
    {
        i8259_enable();
    }

    return 0;
}
