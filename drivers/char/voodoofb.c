#include "vmwarefb.h"
#define log_fmt(fmt) "voodoofb: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/abs.h>
#include <kernel/vga.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>
#include <kernel/framebuffer.h>

// References:
// http://www.bitsavers.org/components/3dfx/Voodoo2_Banshee-2D_Databook_r1.0_199806.pdf

#define DEBUG_VOODOO 1

struct io
{
    io32 status;
    io32 reserved_0[9];
    io32 vga_init0;
    io32 reserved_1[5];
    io32 pll_ctrl0;
    io32 reserved_2[2];
    io32 dac_mode;
    io32 reserved_3[3];

    io32 vid_proc_cfg;
    io32 reserved_4[14];
    io32 vid_screen_size;
    io32 reserved_5[18];
    io32 vid_desktop_start_addr;
    io32 vid_desktop_overlay_stride;
};

typedef struct io io_t;

struct pll
{
    int      m;
    int      n;
    int      k;
    uint32_t freq;
};

typedef struct pll pll_t;

struct mode_setting
{
    uint16_t resx, resy;
    uint32_t pixel_clock;
    uint8_t seq[4];
    uint8_t misc;
    uint8_t crtc[VGA_CRT_C];
    //uint8_t att[20];
    uint8_t att10h;
    uint8_t gfx[9];
} mode_settings[] = {
    {   // FIXME: doesn't work
        // Mode 6B / VESA Mode 107 / *Internal Mode 0Ch*
        // Mode 74 / VESA Mode 11A / Internal Mode 35h
        // Mode 75 / VESA Mode 11B / Internal Mode 36h
        // 1280x1024 - 256-color, 32K-color, 16M-color (8x16 Font)
        // 108.0 MHz, 64 KHz, 60 Hz
        .resx = 1280, .resy = 1024, .pixel_clock = 108000,
        .seq = {
            0x001, 0x00f, 0x000, 0x00e,
        },
        .misc = 0x02f,
        .crtc = {
            0x0ce, 0x09f, 0x0a0, 0x091, 0x0a6, 0x014,
            0x028, 0x052, 0x000, 0x040, 0x000, 0x000,
            0x000, 0x000, 0x000, 0x000, 0x001, 0x004,
            0x0ff, 0x0a0, 0x000, 0x001, 0x028, 0x0e3,
            0x0ff,
        },
        //.att = {
        //    0x000, 0x001, 0x002, 0x003, 0x004, 0x005,
        //    0x006, 0x007, 0x008, 0x009, 0x00a, 0x00b,
        //    0x00c, 0x00d, 0x00e, 0x00f, 0x041, 0x000,
        //    0x00f, 0x000,
        //},
        .att10h = 0x41,
        .gfx = {
            0x000, 0x000, 0x000, 0x000, 0x000, 0x040,
            0x005, 0x00f, 0x0ff,
        }
    },
    {
        // Mode 5Eh / VBE Mode 105h / *Internal Mode 0Fh*
        // Mode 72h / VBE Mode 117h / Internal Mode 33h
        // Mode 73h / VBE Mode 118h / Internal Mode 34h
        // 1024X768 - 256-color, 32K-color, 16M-color (8x16 Font)
        // 65.000 MHz, 48.500 KHz, 60 Hz
        .resx = 1024, .resy = 768, .pixel_clock = 65000,
        .seq = {
            0x001, 0x00f, 0x000, 0x00e,
        },
        .misc = 0x02f,
        .crtc = {
            0x0a3, 0x07f, 0x080, 0x087, 0x083, 0x094,
            0x024, 0x0f5, 0x000, 0x060, 0x000, 0x000,
            0x000, 0x000, 0x000, 0x000, 0x003, 0x009,
            0x0ff, 0x080, 0x000, 0x0ff, 0x025, 0x0e3,
            0x0ff,
        },
        .att10h = 0x41,
        //.att = {
        //    0x000, 0x001, 0x002, 0x003, 0x004, 0x005,
        //    0x006, 0x007, 0x008, 0x009, 0x00a, 0x00b,
        //    0x00c, 0x00d, 0x00e, 0x00f, 0x041, 0x000,
        //    0x00f, 0x000,
        //},
        .gfx = {
            0x000, 0x000, 0x000, 0x000, 0x000, 0x040,
            0x005, 0x00f, 0x0ff,
        }
    },
    {
        // Mode 5Ch / VBE Mode 103h / *Internal Mode 0Bh*
        // Mode 70h / VBE Mode 114h / Internal Mode 31h
        // Mode 71h / VBE Mode 115h / Internal Mode 32h
        // 800X600 - 256-color, 32K-color, 16M-color (8x16 Font)
        // 40.000 MHz, 38.000 KHz, 60 Hz
        .resx = 800, .resy = 600, .pixel_clock = 40000,
        .seq = {
            0x001, 0x00f, 0x000, 0x00e,
        },
        .misc = 0x02f,
        .crtc = {
            0x07f, 0x063, 0x064, 0x082, 0x069, 0x019,
            0x072, 0x0f0, 0x000, 0x060, 0x000, 0x000,
            0x000, 0x000, 0x000, 0x000, 0x059, 0x00d,
            0x057, 0x064, 0x000, 0x058, 0x073, 0x0e3,
            0x0ff,
        },
        .att10h = 0x01,
        //.att = {
        //    0x000, 0x001, 0x002, 0x003, 0x004, 0x005,
        //    0x006, 0x007, 0x008, 0x009, 0x00a, 0x00b,
        //    0x00c, 0x00d, 0x00e, 0x00f, 0x001, 0x000,
        //    0x00f, 0x000,
        //},
        .gfx = {
            0x000, 0x000, 0x000, 0x000, 0x000, 0x040,
            0x005, 0x00f, 0x0ff,
        }
    },
    {
        // Mode 5Fh / VBE Mode 101h / *Internal Mode 09h*
        // Mode 6E / VESA Mode 111h / Internal Mode 2Fh
        // Mode 69 / VESA Mode 112h / Internal Mode 30h
        // 640X480 - 256-color, 32K-color, 16M-color (8x16 Font)
        // 25.175 MHz, 31.5 KHz, 70 Hz
        .resx = 640, .resy = 480, .pixel_clock = 25175,
        .seq = {
            0x001, 0x00f, 0x000, 0x00e,
        },
        .misc = 0x0e3,
        .crtc = {
            0x05f, 0x04f, 0x050, 0x082, 0x052, 0x09e,
            0x00b, 0x03e, 0x000, 0x040, 0x000, 0x000,
            0x000, 0x000, 0x000, 0x000, 0x0ea, 0x00c,
            0x0df, 0x050, 0x000, 0x0e7, 0x004, 0x0e3,
            0x0ff,
        },
        .att10h = 0x41,
        //.att = {
        //    0x000, 0x001, 0x002, 0x003, 0x004, 0x005,
        //    0x006, 0x007, 0x008, 0x009, 0x00a, 0x00b,
        //    0x00c, 0x00d, 0x00e, 0x00f, 0x041, 0x000,
        //    0x00f, 0x000,
        //},
        .gfx = {
            0x000, 0x000, 0x000, 0x000, 0x000, 0x040,
            0x005, 0x00f, 0x0ff,
        }
    }
};

typedef struct mode_setting mode_setting_t;

struct voodoo
{
    mutex_t       lock;
    pci_device_t* pci;
    uintptr_t     fb_paddr;
    void*         fb_vaddr;
    uintptr_t     io_paddr;
    io_t*         io;
    uint16_t      fb_resx;
    uint16_t      fb_resy;
    uint16_t      fb_bpp;
    fb_ops_t      ops;
};

typedef struct voodoo voodoo_t;

enum dac_mode
{
    DAC_MODE_2X = 1 << 0,
};

enum vga_init0
{
    VGA_INIT0_8BIT_CLUT          = 1 << 2,
    VGA_INIT0_VGA_EXT            = 1 << 6,
    VGA_INIT0_WAKEUP_SELECT_3C3  = 1 << 8,
    VGA_INIT0_ALT_READBACK       = 1 << 10,
    VGA_INIT0_EXTENDED_SHIFT_OUT = 1 << 12,
};

enum vid_proc_cfg
{
    VID_PROC_CFG_PROC_ON             = 1 << 0,
    VID_PROC_CFG_DESKTOP_SURFACE     = 1 << 7,
    VID_PROC_CFG_DESKTOP_CLUT_BYPASS = 1 << 10,
    VID_PROC_CFG_PIXEL_FORMAT_16BIT  = 1 << 18,
    VID_PROC_CFG_PIXEL_FORMAT_24BIT  = 2 << 18,
    VID_PROC_CFG_PIXEL_FORMAT_32BIT  = 3 << 18,
};

#define VGA_INIT0_FLAGS \
    (VGA_INIT0_VGA_EXT | VGA_INIT0_WAKEUP_SELECT_3C3 | VGA_INIT0_ALT_READBACK | \
     VGA_INIT0_8BIT_CLUT | VGA_INIT0_EXTENDED_SHIFT_OUT)

#define VID_PROC_CFG_FLAGS \
    (VID_PROC_CFG_PROC_ON | VID_PROC_CFG_DESKTOP_SURFACE | VID_PROC_CFG_DESKTOP_CLUT_BYPASS)

voodoo_t* device;

static int voodoofb_device_id_check(int id)
{
    switch (id)
    {
        case 0x3: // Voodoo Banshee
        case 0x5: // Voodoo 3
            return 0;
        default:
            return -ENODEV;
    }
}

static void voodoofb_fb_setup(void)
{
    uint16_t fb_resx = device->fb_resx;
    uint16_t fb_resy = device->fb_resy;
    uint16_t fb_bpp = device->fb_bpp;
    size_t bytes = fb_bpp_to_bytes(fb_bpp);
    framebuffer.id = "Voodoo FB";
    framebuffer.bpp = fb_bpp;
    framebuffer.paddr = device->fb_paddr;
    framebuffer.vaddr = device->fb_vaddr;
    framebuffer.width = fb_resx;
    framebuffer.height = fb_resy;
    framebuffer.pitch = fb_resx * bytes;
    framebuffer.size = fb_resx * fb_resy * bytes;
    framebuffer.type = FB_TYPE_PACKED_PIXELS;
    framebuffer.type_aux = 0;
    framebuffer.visual = FB_VISUAL_TRUECOLOR;
    framebuffer.accel = false;
    framebuffer.ops = &device->ops;
}

static int voodoofb_fifo_wait(uint32_t entries)
{
    size_t timeout = 1000;
    while (timeout--)
    {
        if ((device->io->status & 0x1f) >= entries)
        {
            return 0;
        }
        mdelay(1);
    }

    return -ETIMEDOUT;
}

static int voodoofb_pixel_format_get(int bpp)
{
    switch (bpp)
    {
        case 16: return VID_PROC_CFG_PIXEL_FORMAT_16BIT;
        case 24: return VID_PROC_CFG_PIXEL_FORMAT_24BIT;
        case 32: return VID_PROC_CFG_PIXEL_FORMAT_32BIT;
        default: return -EINVAL;
    }
}

static mode_setting_t* voodoofb_mode_find(int resx, int resy)
{
    mode_setting_t* m;
    for (size_t i = 0; i < array_size(mode_settings); ++i)
    {
        m = &mode_settings[i];
        if (m->resx == resx && m->resy == resy)
        {
            return m;
        }
    }

    return NULL;
}

static uint32_t voodoofb_pll_freq_khz(pll_t* pll)
{
    //fout = 14.31818 * (N + 2) / (M + 2) / (2 ^ K).
    return (14318 * (pll->n + 2) / (pll->m + 2)) >> pll->k;
}

static uint32_t voodoofb_pll_ctrl0_value(pll_t* pll)
{
    return (pll->n << 8) | (pll->m << 2) | pll->k;
}

static pll_t voodoofb_pll_calculate(mode_setting_t* m)
{
    uint32_t pixel_clock = m->pixel_clock;
    pll_t current = {};
    pll_t best = {};
    uint32_t best_difference = pixel_clock;

    for (current.m = 0; current.m < 64; current.m++)
    {
        for (current.n = 0; current.n < 256; current.n++)
        {
            for (current.k = 0; current.k < 4; current.k++)
            {
                uint32_t freq_khz = voodoofb_pll_freq_khz(&current);
                current.freq = freq_khz;

                uint32_t error = abs((int)(freq_khz - pixel_clock));
                if (error < best_difference)
                {
                    best_difference = error;
                    best = current;
                }
            }
        }
    }

    return best;
}

static int voodoofb_fb_mode_set(int resx, int resy, int bpp)
{
    int pixel_format = voodoofb_pixel_format_get(bpp);

    if (unlikely(pixel_format < 0))
    {
        return pixel_format;
    }

    mode_setting_t* m = voodoofb_mode_find(resx, resy);

    if (unlikely(!m))
    {
        return -EINVAL;
    }

    pll_t pll = voodoofb_pll_calculate(m);

    log_debug(DEBUG_VOODOO, "pll: %u kHz, m=%d, n=%d, k=%d", pll.freq, pll.m, pll.n, pll.k);

    scoped_mutex_lock(&device->lock);

    voodoofb_fifo_wait(2);
    device->io->vid_proc_cfg = 0;
    device->io->pll_ctrl0 = voodoofb_pll_ctrl0_value(&pll);

    vga_misc_write(m->misc);

    for (size_t i = 0; i < sizeof(m->seq); ++i)
    {
        vga_seq_write(i + 1, m->seq[i]);
    }

    vga_crt_write(VGA_CRTC_CR11, vga_crt_read(VGA_CRTC_CR11) & ~VGA_CR11_LOCK_CR0_CR7);

    for (size_t i = 0; i < sizeof(m->crtc); ++i)
    {
        vga_crt_write(i, m->crtc[i]);
    }

    for (size_t i = 0; i < sizeof(m->gfx); ++i)
    {
        vga_gfx_write(i, m->gfx[i]);
    }

    vga_is1_read();
    vga_att_write(VGA_ATC_MODE, m->att10h);

    mdelay(50);

    vga_is1_read();
    outb(0x20, VGA_ATT_IW);

    voodoofb_fifo_wait(6);
    device->io->vga_init0 = VGA_INIT0_FLAGS;
    device->io->dac_mode = 0;
    device->io->vid_desktop_overlay_stride = resx * fb_bpp_to_bytes(bpp);
    device->io->vid_screen_size = resx | (resy << 12);
    device->io->vid_desktop_start_addr = 0;
    device->io->vid_proc_cfg = VID_PROC_CFG_FLAGS | pixel_format;

    device->fb_resx = resx;
    device->fb_resy = resy;
    device->fb_bpp = bpp;

    voodoofb_fb_setup();

    return 0;
}

UNMAP_AFTER_INIT int voodoofb_init(void)
{
    int errno;
    pci_device_t* pci_device;

    if (!(pci_device = pci_device_get(PCI_DISPLAY, PCI_DISPLAY_VGA)))
    {
        return -ENODEV;
    }

    if (pci_device->vendor_id != PCI_3DFX || voodoofb_device_id_check(pci_device->device_id))
    {
        return -ENODEV;
    }

    if (unlikely(pci_device_initialize(pci_device)))
    {
        return -EIO;
    }

    if (unlikely(!(device = zalloc(voodoo_t))))
    {
        return -ENOMEM;
    }

    device->pci = pci_device;
    device->ops.mode_set = &voodoofb_fb_mode_set;
    mutex_init(&device->lock);

    device->io_paddr = pci_device->bar[0].addr;
    device->fb_paddr = pci_device->bar[1].addr;

    device->fb_vaddr = mmio_map_wc(device->fb_paddr, 16 * MiB, "voodoo_fb");

    if (unlikely(!device->fb_vaddr))
    {
        log_notice("cannot map FB");
        errno = -ENOMEM;
        goto error;
    }

    device->io = mmio_map_uc(device->io_paddr, PAGE_SIZE, "voodoo_io");

    if (unlikely(!device->io))
    {
        log_notice("cannot map IO");
        errno = -ENOMEM;
        goto error;
    }

    if (unlikely(errno = voodoofb_fb_mode_set(1024, 768, 32)))
    {
        log_warning("failed to set mode");
        goto error;
    }

    return 0;

error:
    if (!device) return errno;
    if (device->io) mmio_unmap(device->io);
    if (device->fb_vaddr) mmio_unmap(device->fb_vaddr);
    delete(device);
    device = NULL;
    return errno;
}
