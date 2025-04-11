#define log_fmt(fmt) "xhci: " fmt
#include "xhci.h"

#include <arch/pci.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

#include "usb.h"
#include "usb_device.h"

static_assert(sizeof(raw_trb_t) == 16);
static_assert(sizeof(ring_t) < PAGE_SIZE);

static int xhci_init(usb_hc_t* hc, pci_device_t* pci_device);

READONLY static usb_hc_ops_t ops = {
    .name = "xhci",
    .init = &xhci_init,
};

#define WAIT_WHILE(condition) \
    ({ \
        int timeout = 5000; \
        while ((condition) && --timeout) \
        { \
            udelay(100); \
        } \
        !timeout; \
    })

static void intel_ehci_find(pci_device_t* pci_device, void* data)
{
    size_t* ehci_count = data;

    if (pci_device->class == PCI_SERIAL_BUS &&
        pci_device->subclass == PCI_SERIAL_BUS_USB &&
        pci_device->prog_if == PCI_USB_EHCI &&
        pci_device->vendor_id == PCI_INTEL)
    {
        (*ehci_count)++;
    }
}

struct xhci_intel_pci
{
    uint32_t xusb2pr;
    uint32_t xusb2prm;
    uint32_t usb3_pssen;
    uint32_t usb3prm;
};

typedef struct xhci_intel_pci xhci_intel_pci_t;

static int intel_xhci_handle(pci_device_t* pci_device)
{
    size_t ehci_count = 0;
    pci_device_enumerate(&intel_ehci_find, &ehci_count);

    if (!ehci_count)
    {
        return 0;
    }

    log_notice("%u Intel EHCI controllers", ehci_count);

    xhci_intel_pci_t intel_pci;

    pci_config_read(pci_device, 0xd0, &intel_pci, sizeof(intel_pci));

    log_notice("XUSB2PR: %#x, XUSB2PRM: %#x, USB3_PSSEN: %#x, USB3PRM: %#x",
        intel_pci.xusb2pr,
        intel_pci.xusb2prm,
        intel_pci.usb3_pssen,
        intel_pci.usb3prm);

    pci_config_writel(pci_device, 0xd8, 0);
    pci_config_writel(pci_device, 0xd0, 0);
    pci_config_writel(pci_device, PCI_REG_COMMAND, 0);

    return 1;
}

static int xhci_cmd_ring_setup(page_t** result_page, ring_t** result_ring)
{
    page_t* page = page_alloc(1, PAGE_ALLOC_ZEROED | PAGE_ALLOC_UNCACHED);

    if (unlikely(!page))
    {
        return -ENOMEM;
    }

    uintptr_t paddr = page_phys(page);
    ring_t* ring = page_virt_ptr(page);

    for (size_t i = 0; i < RING_SIZE; ++i)
    {
        ring->trbs[i].raw_trb = &ring->raw_trbs[i];
        ring->trbs[i].paddr = paddr;
        ring->trbs[i].next = &ring->trbs[i + 1];
        ring->trbs[i].prev = &ring->trbs[i - 1];
        paddr += sizeof(*ring->trbs);
    }

    ring->trbs[0].prev = &ring->trbs[RING_SIZE - 1];
    ring->trbs[RING_SIZE - 1].next = &ring->trbs[0];
    ring->raw_trbs[RING_SIZE - 1].buffer_addr = ring->trbs[0].paddr;
    ring->raw_trbs[RING_SIZE - 1].status = TRB_C | TRB_TYPE_LINK;

    ring->dequeue_ptr = ring->enqueue_ptr = &ring->trbs[0];
    ring->cycle_state = TRB_C;

    *result_page = page;
    *result_ring = ring;

    return 0;
}

static int xhci_hc_reset(xhci_t* xhci)
{
    xhci_usbcmd_write(xhci, USBCMD_HCRST);

    if (unlikely(WAIT_WHILE(xhci_usbcmd_read(xhci) & USBCMD_HCRST)))
    {
        log_notice("[hc] reset: timeout");
        return -ETIMEDOUT;
    }

    return 0;
}

static int xhci_hc_initialize(xhci_t* xhci)
{
    int errno;

    xhci_usbsts_write(xhci, xhci_usbsts_read(xhci));
    xhci_usbcmd_write(xhci, 0);

    if (unlikely(WAIT_WHILE(!(xhci_usbsts_read(xhci) & USBSTS_HCH))))
    {
        log_notice("[hc] stop: timeout");
        return -ETIMEDOUT;
    }

    if (unlikely(errno = xhci_hc_reset(xhci)))
    {
        return errno;
    }

    xhci_crcr_write(xhci, xhci->cmd_ring->enqueue_ptr->paddr | xhci->cmd_ring->cycle_state);
    xhci_config_write(xhci, 4);
    xhci_usbcmd_write(xhci, USBCMD_RUN);

    return 0;
}

UNMAP_AFTER_INIT static int xhci_init(usb_hc_t* hc, pci_device_t* pci_device)
{
    int errno;

    if (pci_device->class != PCI_SERIAL_BUS ||
        pci_device->subclass != PCI_SERIAL_BUS_USB ||
        pci_device->prog_if != PCI_USB_xHCI)
    {
        log_error("invalid device: class %02x%02x, prog_if: %#x",
            pci_device->class,
            pci_device->subclass,
            pci_device->prog_if);
        return -ENODEV;
    }

    if (pci_device->vendor_id == PCI_INTEL)
    {
        if (intel_xhci_handle(pci_device))
        {
            return -ENODEV;
        }
    }

    if (unlikely(!pci_device->bar[0].addr || !pci_device->bar[0].size))
    {
        return -EINVAL;
    }

    void* mmio = mmio_map_uc(pci_device->bar[0].addr, pci_device->bar[0].size, "xhci");

    if (unlikely(!mmio))
    {
        log_notice("cannot map MMIO: %p", pci_device->bar[0].addr);
        return -ENOMEM;
    }

    xhci_t* xhci = alloc(xhci_t);

    if (unlikely(!xhci))
    {
        mmio_unmap(mmio);
        return -ENOMEM;
    }

    xhci->cap_base = mmio;
    xhci->oper_base = mmio + xhci_caplength_read(xhci);
    xhci->db_base = mmio + xhci_dboff_read(xhci);
    xhci->rt_base = mmio + xhci_rtsoff_read(xhci);

    if (unlikely(errno = xhci_cmd_ring_setup(&xhci->cmd_ring_page, &xhci->cmd_ring)))
    {
        return errno;
    }

    if (unlikely(errno = xhci_cmd_ring_setup(&xhci->event_ring_page, &xhci->event_ring)))
    {
        return errno;
    }

    if (unlikely(errno = xhci_cmd_ring_setup(&xhci->transfer_ring_page, &xhci->transfer_ring)))
    {
        return errno;
    }

    log_notice("HCIVERSION: %#x, HCSPARAMS: %#x %#x %#x",
        xhci_hciversion_read(xhci),
        xhci_hcsparams1_read(xhci),
        xhci_hcsparams2_read(xhci),
        xhci_hcsparams3_read(xhci));

    log_notice("HCCPARAMS: %#x %#x",
        xhci_hccparams1_read(xhci),
        xhci_hccparams2_read(xhci));

    xhci_hc_initialize(xhci);

    log_notice("USBCMD: %#x, USBSTS: %#x, PAGESIZE: %#x",
        xhci_usbcmd_read(xhci),
        xhci_usbsts_read(xhci),
        xhci_pagesize_read(xhci));

    hc->data = xhci;

    return -ENODEV;
}

UNMAP_AFTER_INIT int xhci_register(void)
{
    return usb_hc_driver_register(&ops, PCI_USB_xHCI);
}

premodules_initcall(xhci_register);
