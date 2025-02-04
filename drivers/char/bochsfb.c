#define log_fmt(fmt) "bochsfb: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/vga.h>
#include <kernel/vga.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>
#include <kernel/framebuffer.h>

// References:
// https://wiki.osdev.org/Bochs_VBE_Extensions
// https://github.com/qemu/vgabios/blob/master/vbe_display_api.txt
// https://www.qemu.org/docs/master/specs/standard-vga.html

#define DEBUG_BOCHS 1

#define VBE_DISPI_IOPORT_INDEX 0x01ce
#define VBE_DISPI_IOPORT_DATA  0x01cf

#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

#define VBE_DISPI_ID0    0xb0c0
#define VBE_DISPI_ID1    0xb0c1
#define VBE_DISPI_ID2    0xb0c2
#define VBE_DISPI_ID3    0xb0c3
#define VBE_DISPI_ID4    0xb0c4
#define VBE_DISPI_ID5    0xb0c5

#define VBE_DISPI_BPP_4  0x04
#define VBE_DISPI_BPP_8  0x08
#define VBE_DISPI_BPP_15 0x0f
#define VBE_DISPI_BPP_16 0x10
#define VBE_DISPI_BPP_24 0x18
#define VBE_DISPI_BPP_32 0x20

#define VBE_DISPI_DISABLED    0x00
#define VBE_DISPI_ENABLED     0x01
#define VBE_DISPI_LFB_ENABLED 0x40

struct bochs
{
    mutex_t       lock;
    pci_device_t* pci;
    uintptr_t     fb_paddr;
    void*         fb_vaddr;
    void*         io_vaddr;
    fb_ops_t      ops;
};

typedef struct bochs bochs_t;

static bochs_t* device;

static uint16_t bochsfb_read(uint16_t reg)
{
    if (device && device->io_vaddr)
    {
        int offset = 0x500 + (reg << 1);
        return readw(device->io_vaddr + offset);
    }
    else
    {
        outw(reg, VBE_DISPI_IOPORT_INDEX);
        return inw(VBE_DISPI_IOPORT_DATA);
    }
}

static void bochsfb_write(uint16_t reg, uint16_t data)
{
    if (device && device->io_vaddr)
    {
        int offset = 0x500 + (reg << 1);
        writew(data, device->io_vaddr + offset);
    }
    else
    {
        outw(reg, VBE_DISPI_IOPORT_INDEX);
        outw(data, VBE_DISPI_IOPORT_DATA);
    }
}

static void bochsfb_fb_setup(uint16_t resx, uint16_t resy, uint16_t bpp)
{
    size_t bytes = fb_bpp_to_bytes(bpp);

    framebuffer.id = "Bochs FB";
    framebuffer.bpp = bpp;
    framebuffer.paddr = device->fb_paddr;
    framebuffer.vaddr = device->fb_vaddr;
    framebuffer.width = resx;
    framebuffer.height = resy;
    framebuffer.pitch = resx * bytes;
    framebuffer.size = resx * resy * bytes;
    framebuffer.type = FB_TYPE_PACKED_PIXELS;
    framebuffer.type_aux = 0;
    framebuffer.visual = bpp == 8 ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
    framebuffer.accel = false;
    framebuffer.ops = &device->ops;
}

static int bochsfb_pixel_format_get(int bpp)
{
    switch (bpp)
    {
        case 8:  return VBE_DISPI_BPP_8;
        case 15: return VBE_DISPI_BPP_15;
        case 16: return VBE_DISPI_BPP_16;
        case 24: return VBE_DISPI_BPP_24;
        case 32: return VBE_DISPI_BPP_32;
        default: return -EINVAL;
    }
}

static int bochsfb_fb_mode_set(int resx, int resy, int bpp)
{
    int pixel_format = bochsfb_pixel_format_get(bpp);

    if (unlikely(pixel_format < 0))
    {
        return pixel_format;
    }

    scoped_mutex_lock(&device->lock);

    bochsfb_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bochsfb_write(VBE_DISPI_INDEX_XRES, resx);
    bochsfb_write(VBE_DISPI_INDEX_YRES, resy);
    bochsfb_write(VBE_DISPI_INDEX_BPP, pixel_format);
    bochsfb_write(VBE_DISPI_INDEX_X_OFFSET, 0);
    bochsfb_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bochsfb_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    bochsfb_fb_setup(resx, resy, bpp);

    return 0;
}

UNMAP_AFTER_INIT int bochsfb_init(void)
{
    int errno;
    pci_device_t* pci_device;

    if (unlikely(!(pci_device = pci_device_get(PCI_DISPLAY, PCI_DISPLAY_VGA))))
    {
        return -ENODEV;
    }

    if (unlikely(pci_device->vendor_id != PCI_QEMU || pci_device->device_id != 0x1111))
    {
        return -ENODEV;
    }

    uint16_t version = bochsfb_read(VBE_DISPI_INDEX_ID);

    log_notice("version: %#x", version);

    if (unlikely(version < VBE_DISPI_ID2))
    {
        return -ENODEV;
    }

    if (unlikely(!(device = zalloc(bochs_t))))
    {
        return -ENOMEM;
    }

    device->pci = pci_device;
    device->ops.mode_set = &bochsfb_fb_mode_set;
    mutex_init(&device->lock);

    device->fb_paddr = pci_device->bar[0].addr;
    device->fb_vaddr = mmio_map_wc(device->fb_paddr, 16 * MiB, "bochs_fb");

    if (unlikely(!device->fb_vaddr))
    {
        log_notice("cannot map FB");
        errno = -ENOMEM;
        goto error;
    }

    if (pci_device->bar[2].addr)
    {
        device->io_vaddr = mmio_map_uc(pci_device->bar[2].addr, pci_device->bar[2].size, "bochs_io");

        if (!device->io_vaddr)
        {
            log_notice("failed to map IO; will use IO ports");
        }
    }

    if (unlikely(errno = bochsfb_fb_mode_set(1024, 768, 32)))
    {
        log_warning("failed to set mode");
        goto error;
    }

    return 0;

error:
    if (!device) return errno;
    if (device->fb_vaddr) mmio_unmap(device->fb_vaddr);
    if (device->io_vaddr) mmio_unmap(device->io_vaddr);
    delete(device);
    device = NULL;
    return errno;
}
