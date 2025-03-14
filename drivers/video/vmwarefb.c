#define log_fmt(fmt) "vmwarefb: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/timer.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>
#include <kernel/framebuffer.h>

#include "vmwarefb.h"
#include "video_driver.h"

static int vmwarefb_mode_set(int resx, int resy, int bpp);

static svga_t svga;

static fb_ops_t fb_ops = {
    .mode_set = &vmwarefb_mode_set
};

static uint32_t svga_read(uint16_t reg)
{
    outl(reg, svga.iobase + SVGA_INDEX);
    return inl(svga.iobase + SVGA_VALUE);
}

static void svga_write(uint16_t reg, uint32_t val)
{
    outl(reg, svga.iobase + SVGA_INDEX);
    outl(val, svga.iobase + SVGA_VALUE);
}

static void vmwarefb_fb_setup(void)
{
    framebuffer.id = "VMWare SVGA FB";
    framebuffer.bpp = svga.bpp;
    framebuffer.paddr = svga.fb_paddr + svga.fb_offset;
    framebuffer.vaddr = svga.fb_vaddr + svga.fb_offset;
    framebuffer.width = svga.resx;
    framebuffer.height = svga.resy;
    framebuffer.pitch = svga.resx * 4;
    framebuffer.size = svga.resx * svga.resy * 4;
    framebuffer.type = FB_TYPE_PACKED_PIXELS;
    framebuffer.type_aux = 0;
    framebuffer.visual = FB_VISUAL_TRUECOLOR;
    framebuffer.accel = false;
    framebuffer.ops = &fb_ops;
}

static int vmwarefb_mode_set(int resx, int resy, int bpp)
{
    if (unlikely(resx < 0 || resy < 0))
    {
        return -EINVAL;
    }

    if (!(svga.cap & SVGA_CAP_8BIT_EMULATION) && bpp != 32)
    {
        return -EINVAL;
    }

    svga_write(SVGA_REG_ENABLE, 0);

    svga.resx = resx;
    svga.resy = resy;
    svga.bpp = bpp;

    svga_write(SVGA_REG_WIDTH, resx);
    svga_write(SVGA_REG_HEIGHT, resy);
    if (svga.cap & SVGA_CAP_8BIT_EMULATION)
    {
        svga_write(SVGA_REG_BPP, bpp);
    }

    svga_write(SVGA_REG_ENABLE, 1);

    svga.fb_offset = svga_read(SVGA_REG_FB_OFFSET);

    vmwarefb_fb_setup();

    return 0;
}

static void vmwarefb_flush(ktimer_t*)
{
    svga.fifo->start = 16;
    svga.fifo->size = 16 + (10 * 1024);
    svga.fifo->next_command = 16 + 4 * 5;
    svga.fifo->stop = 16;
    svga.fifo->commands[0] = SVGA_CMD_UPDATE;
    svga.fifo->commands[1] = 0;
    svga.fifo->commands[2] = 0;
    svga.fifo->commands[3] = svga.resx;
    svga.fifo->commands[4] = svga.resy;
    svga_write(SVGA_REG_SYNC, 1);
}

UNMAP_AFTER_INIT int vmwarefb_init(void)
{
    pci_device_t* pci_device;

    if (unlikely(!(pci_device = pci_device_get(PCI_DISPLAY, PCI_DISPLAY_VGA)) ||
        pci_device->vendor_id != PCI_VENDOR_ID_VMWARE ||
        pci_device->device_id != PCI_DEVICE_ID_VMWARE_SVGA2))
    {
        log_info("no device");
        return -ENODEV;
    }

    if (unlikely(pci_device_initialize(pci_device)))
    {
        log_warning("failed to initialize PCI device");
        return -ENODEV;
    }

    svga.iobase = pci_device->bar[0].addr;

    svga_write(SVGA_REG_ID, SVGA_ID_2);

    if (svga_read(SVGA_REG_ID) != SVGA_ID_2)
    {
        log_info("v2 is not supported");
        return -ENODEV;
    }

    svga.fb_paddr = svga_read(SVGA_REG_FB_START);
    svga.fb_size = svga_read(SVGA_REG_FB_SIZE);
    svga.cap = svga_read(SVGA_REG_CAPABILITIES);
    svga.fifo_paddr = svga_read(SVGA_REG_FIFO_START);
    svga.fifo_size = svga_read(SVGA_REG_FIFO_SIZE);

    svga.fb_vaddr = mmio_map_wc(svga.fb_paddr, svga.fb_size, "fb");

    if (unlikely(!svga.fb_vaddr))
    {
        return -ENOMEM;
    }

    memset(svga.fb_vaddr, 0, svga.fb_size);

    vmwarefb_mode_set(1920, 1080, 32);

    svga.fifo = mmio_map_uc(svga.fifo_paddr, svga.fifo_size, "svga_fifo");

    if (unlikely(!svga.fifo))
    {
        mmio_unmap(svga.fb_vaddr);
        return -ENOMEM;
    }

    svga.fifo->start        = 16;
    svga.fifo->size         = 16 + (10 * 1024);
    svga.fifo->next_command = 16;
    svga.fifo->stop         = 16;

    svga_write(SVGA_REG_CONFIG_DONE, 1);

    ktimer_create_and_start(
        KTIMER_REPEATING,
        (timeval_t){.tv_sec = 0, .tv_usec = USEC_IN_SEC / 60},
        &vmwarefb_flush,
        NULL);

    return 0;
}

READONLY static video_driver_t driver = {
    .name = "vmware",
    .score = 30,
    .initialize = &vmwarefb_init,
};

UNMAP_AFTER_INIT static int vmware_driver_register(void)
{
    return video_driver_register(&driver);
}

premodules_initcall(vmware_driver_register);
