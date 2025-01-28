#define log_fmt(fmt) "vesafb: " fmt
#include <kernel/init.h>
#include <kernel/unit.h>
#include <kernel/vesa.h>
#include <kernel/kernel.h>
#include <kernel/api/ioctl.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>
#include <kernel/framebuffer.h>

#define VBE_INFO_BLOCK_ADDR         0x1000
#define VBE_MODE_INFO_BLOCK_ADDR    0x1300
#define VBE_EDID_ADDR               0x1400

struct mode_info
{
    union
    {
        struct { uint16_t resx, resy; };
        uint32_t is_valid;
    };

    uintptr_t fb_paddr;
    size_t    fb_size;
    uint16_t  pitch;
    uint16_t  mode;
    uint8_t   bits:6;
    uint8_t   type:1;
    uint8_t   color:1;
    uint8_t   has_lfb:1;
    uint8_t   memory_model;
};

typedef struct mode_info mode_info_t;
typedef struct mapped_fb mapped_fb_t;

static int vesafb_framebuffer_setup(mode_info_t* mode);
static int vesafb_fb_mode_set(int resx, int resy, int bpp);

static mode_info_t* desired_mode;
static mode_info_t* current_mode;
static mode_info_t* modes;
static size_t total_memory;

static struct
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
};

static fb_ops_t vesafb_ops = {
    .mode_set = &vesafb_fb_mode_set,
};

static const char* vesafb_memory_model_string(uint8_t model)
{
    switch (model)
    {
        case VBE_MODE_MEMORY_MODEL_TEXT:     return "Text";
        case VBE_MODE_MEMORY_MODEL_CGA:      return "CGA";
        case VBE_MODE_MEMORY_MODEL_HERCULES: return "Hercules";
        case VBE_MODE_MEMORY_MODEL_PLANAR:   return "Planar";
        case VBE_MODE_MEMORY_MODEL_PACKED:   return "Packed";
        case VBE_MODE_MEMORY_MODEL_NONCHAIN: return "Non-chain";
        case VBE_MODE_MEMORY_MODEL_DIRECT:   return "Direct";
        case VBE_MODE_MEMORY_MODEL_YUV:      return "YUV";
        default:                             return "Unknown";
    }
}

static const char* vesafb_mode_string(mode_info_t* m, char* buf, size_t size)
{
    int i;
    i = snprintf(buf, size, "%#06x:% 5d x% 5d x% 3d ", m->mode, m->resx, m->resy, m->bits);
    i += snprintf(buf + i, size - i, "%s %s",
        m->type == VBE_MODE_GRAPHICS ? "graphics" : "text",
        m->color ? "color" : "mono");

    if (m->has_lfb)
    {
        i += snprintf(buf + i, size - i, " fb=%#x", m->fb_paddr);
    }

    snprintf(buf + i, size - i, " %s", vesafb_memory_model_string(m->memory_model));

    return buf;
}

static int vesafb_mode_set_raw(mode_info_t* mode)
{
    regs_t regs;
    regs.ax = VBE_SET_MODE;
    regs.bx = mode->mode | (mode->has_lfb ? VBE_MODE_LINEAR_FB : 0);
    vbe_call(&regs);

    if (regs.ax != VBE_SUPPORTED)
    {
        log_warning("setting mode failed");
        return -EIO;
    }

    return 0;
}

static int vesafb_mode_set(mode_info_t* mode)
{
    int errno;
    char buf[128];
    mode_info_t* prev_mode = current_mode;

    log_info("setting: %s", vesafb_mode_string(mode, buf, sizeof(buf)));

    if (unlikely(errno = vesafb_mode_set_raw(mode)))
    {
        return errno;
    }

    if (unlikely(errno = vesafb_framebuffer_setup(mode)))
    {
        if (prev_mode)
        {
            vesafb_mode_set_raw(prev_mode);
        }
        return errno;
    }

    current_mode = mode;

    return 0;
}

static uint16_t vesafb_mode_read(void)
{
    regs_t regs;

    regs.ax = VBE_GET_MODE;

    if (unlikely(vbe_call(&regs)))
    {
        log_warning("cannot read mode: %#x", regs.ax);
        return 0;
    }

    return regs.bx & 0x7fff;
}

static int vesafb_modes_print(void)
{
    mode_info_t* v;
    char buf[128];

    for (v = modes; v->is_valid; ++v)
    {
        log_notice("%s", vesafb_mode_string(v, buf, sizeof(buf)));
    }
    return 0;
}

static void vesafb_edid_read(vbe_info_block_t* vbe, int* resx, int* resy)
{
    regs_t regs;

    if (vbe->version < 0x300)
    {
        return;
    }

    vbe_call(VBE_EDID_GET(regs));

    if (regs.al != 0x4f || regs.ah == 1 || *(uint64_t*)VBE_EDID_ADDR != EDID_SIGNATURE)
    {
        log_info("EDID reading unsupported/failed; al=%#x ah=%#x sig=%#llx",
            regs.al,
            regs.ah,
            *(uint64_t*)VBE_EDID_ADDR);
        return;
    }

    uint8_t* edid = ptr(VBE_EDID_ADDR);
    *resx = edid[0x38] | ((int)(edid[0x3a] & 0xf0) << 4);
    *resy = edid[0x3b] | ((int)(edid[0x3d] & 0xf0) << 4);

    log_info("edid: %ux%u", *resx, *resy);
}

static void vesafb_mode_fill(
    mode_info_t* mode,
    uint16_t vesafb_mode,
    const vbe_mib_t* vesafb_mode_info)
{
    mode->resx         = vesafb_mode_info->resx;
    mode->resy         = vesafb_mode_info->resy;
    mode->pitch        = vesafb_mode_info->pitch;
    mode->mode         = vesafb_mode;
    mode->bits         = vesafb_mode_info->bits_per_pixel;
    mode->type         = vesafb_mode_info->mode_attr.type;
    mode->color        = vesafb_mode_info->mode_attr.color;
    mode->has_lfb      = vesafb_mode_info->mode_attr.linear_framebuffer;
    mode->memory_model = vesafb_mode_info->memory_model;

    if (mode->has_lfb)
    {
        mode->fb_paddr = vesafb_mode_info->framebuffer;
        mode->fb_size  = total_memory;
    }
    else
    {
        mode->fb_paddr = vesafb_mode_info->wina_segment * 0x10;
        mode->fb_size  = vesafb_mode_info->win_size;
    }
}

static mode_info_t* vesafb_mode_find(void)
{
    size_t prio = ~0UL;
    mode_info_t* desired = NULL;

    for (mode_info_t* m = modes; m->is_valid; ++m)
    {
        for (size_t i = 0; i < array_size(res_priorities); ++i)
        {
            if (i < prio &&
                m->resx == res_priorities[i].resx &&
                m->resy == res_priorities[i].resy &&
                m->bits == res_priorities[i].bits &&
                m->has_lfb)
            {
                prio = i;
                desired = m;
            }
        }
    }

    return desired;
}

static int vesafb_fb_mode_set(int resx, int resy, int bpp)
{
    for (mode_info_t* m = modes; m->is_valid; ++m)
    {
        if (m->resx == resx && m->resy == resy && m->bits == bpp)
        {
            return vesafb_mode_set(m);
        }
    }

    return -EINVAL;
}

static bool vesafb_is_mode_supported(mode_info_t* mode)
{
    if (mode->memory_model == VBE_MODE_MEMORY_MODEL_TEXT ||
        mode->memory_model == VBE_MODE_MEMORY_MODEL_DIRECT)
    {
        if (mode->type == VBE_MODE_GRAPHICS && mode->has_lfb)
        {
            return true;
        }
        else if (mode->type == VBE_MODE_TEXT)
        {
            return true;
        }
    }

    return false;
}

static int vesafb_framebuffer_setup(mode_info_t* mode)
{
    size_t size = mode->fb_size;
    uintptr_t prev_paddr = framebuffer.paddr;

    switch (mode->memory_model)
    {
        case VBE_MODE_MEMORY_MODEL_TEXT:
            framebuffer.type = FB_TYPE_TEXT;
            framebuffer.type_aux = FB_AUX_TEXT_CGA;
            framebuffer.visual = FB_VISUAL_PSEUDOCOLOR;
            break;
        case VBE_MODE_MEMORY_MODEL_CGA:
        case VBE_MODE_MEMORY_MODEL_HERCULES:
        case VBE_MODE_MEMORY_MODEL_PLANAR:
        case VBE_MODE_MEMORY_MODEL_NONCHAIN:
        case VBE_MODE_MEMORY_MODEL_YUV:
            return -ENODEV;
        case VBE_MODE_MEMORY_MODEL_PACKED:
        case VBE_MODE_MEMORY_MODEL_DIRECT:
            framebuffer.type = FB_TYPE_PACKED_PIXELS;
            framebuffer.visual = mode->bits == 8 ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
            framebuffer.type_aux = 0;
            break;
    }

    framebuffer.id       = "VESA FB";
    framebuffer.pitch    = mode->pitch;
    framebuffer.width    = mode->resx;
    framebuffer.height   = mode->resy;
    framebuffer.bpp      = mode->bits;
    framebuffer.size     = framebuffer.pitch * framebuffer.height;
    framebuffer.paddr    = mode->fb_paddr;
    framebuffer.accel    = FB_ACCEL_NONE;
    framebuffer.ops      = &vesafb_ops;

    if (prev_paddr != framebuffer.paddr)
    {
        if (framebuffer.vaddr)
        {
            mmio_unmap(framebuffer.vaddr);
        }
        framebuffer.vaddr = mmio_map_wc(framebuffer.paddr, size, "fb");
        if (unlikely(!framebuffer.vaddr))
        {
            log_error("cannot map framebuffer");
            return -ENOMEM;
        }
    }

    return 0;
}

int vesafb_initialize(void)
{
    int resx = 0, resy = 0;
    size_t index = 0;
    regs_t regs;
    page_t* page;
    char buf[128];
    vbe_info_block_t* vbe = ptr(VBE_INFO_BLOCK_ADDR);
    vbe_mib_t* mode_info = ptr(VBE_MODE_INFO_BLOCK_ADDR);

    if (vbe_call(VBE_MODE_GET(regs)))
    {
        log_info("VBE_GET_MODE_INFO failed with %#x", regs.ah);
        return -ENOSYS;
    }

    if (vbe->version < 0x200)
    {
        log_warning("too old version: %#x", vbe->version);
        return -ENOSYS;
    }

    log_notice("product name: %s; version: %#x", farptr(vbe->oem_product_name), vbe->version);

    page = page_alloc(1, PAGE_ALLOC_ZEROED);

    if (unlikely(!page))
    {
        return -ENOMEM;
    }

    total_memory = vbe->total_memory * 64 * KiB;
    modes = page_virt_ptr(page);

    vesafb_edid_read(vbe, &resx, &resy);

    uint16_t current_vesafb_mode = vesafb_mode_read();

    for (uint16_t* mode_ptr = farptr(vbe->video_mode); *mode_ptr != VBE_MODE_END; ++mode_ptr)
    {
        mode_info_t* mode;
        regs.ax = VBE_GET_MODE_INFO;
        regs.cx = *mode_ptr;
        regs.di = VBE_MODE_INFO_BLOCK_ADDR;
        vbe_call(&regs);

        if (regs.ax != VBE_SUPPORTED || !mode_info->mode_attr.supported)
        {
            continue;
        }

        mode = &modes[index++];
        vesafb_mode_fill(mode, *mode_ptr, mode_info);

        if (mode->mode == current_vesafb_mode)
        {
            current_mode = mode;
        }

        if (resx &&
            mode->resx == resx &&
            mode->resy == resy)
        {
            if (!desired_mode || mode->bits > desired_mode->bits)
            {
                desired_mode = mode;
            }
        }
    }

    log_notice("supported modes: %u", index);

    param_call_if_set(KERNEL_PARAM("vesaprint"), &vesafb_modes_print);

    if (current_mode)
    {
        log_notice("current: %s", vesafb_mode_string(current_mode, buf, sizeof(buf)));
    }

    desired_mode = desired_mode ? : vesafb_mode_find();

    if (desired_mode && !param_bool_get(KERNEL_PARAM("nomodeset")))
    {
        if (vesafb_is_mode_supported(desired_mode))
        {
            vesafb_mode_set(desired_mode);
        }
        else
        {
            log_warning("incompatible mode: %#x type %#x fb %#x",
                desired_mode->mode,
                desired_mode->type,
                desired_mode->fb_paddr);
        }
    }

    if (unlikely(!current_mode))
    {
        return -ENODEV;
    }

    return 0;
}
