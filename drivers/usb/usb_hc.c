#define log_fmt(fmt) "usb-hc: " fmt
#include "usb_hc.h"

#include <arch/pci.h>
#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/process.h>

#include "usb.h"

module_init(usb_hc_init);
module_exit(usb_hc_deinit);
KERNEL_MODULE(usb_hc);

struct usb_hc_driver
{
    int           type;
    usb_hc_ops_t* ops;
    int           next_hc_id;
    list_head_t   list_entry;
};

typedef struct usb_hc_driver usb_hc_driver_t;

READONLY static LIST_DECLARE(hc_drivers);
static LIST_DECLARE(hcs);

static const char* usb_hc_type_string(int type)
{
    switch (type)
    {
        case PCI_USB_UHCI: return "UHCI";
        case PCI_USB_OHCI: return "OHCI";
        case PCI_USB_EHCI: return "EHCI";
        case PCI_USB_xHCI: return "xHCI";
        default:           return "<unknown>";
    }
}

static usb_hc_driver_t* usb_hc_driver_find(int type)
{
    usb_hc_driver_t* hc_driver;

    list_for_each_entry(hc_driver, &hc_drivers, list_entry)
    {
        if (hc_driver->type == type)
        {
            return hc_driver;
        }
    }

    return NULL;
}

UNMAP_AFTER_INIT static void usb_hc_init_single(pci_device_t* pci_device, void* data)
{
    usb_hc_driver_t* hc_driver = data;
    int type = hc_driver->type;

    if (pci_device->class != PCI_SERIAL_BUS ||
        pci_device->subclass != PCI_SERIAL_BUS_USB ||
        type != pci_device->prog_if)
    {
        return;
    }

    if (unlikely(pci_device_initialize(pci_device)))
    {
        log_warning("[%s %04x:%04x] cannot initialize PCI device",
            usb_hc_type_string(hc_driver->type),
            pci_device->vendor_id,
            pci_device->device_id);
        return;
    }

    char* vendor_id;
    char* device_id;

    pci_device_describe(pci_device, &vendor_id, &device_id);

    log_info("[%s %04x:%04x] %s %s",
        usb_hc_type_string(type),
        pci_device->vendor_id,
        pci_device->device_id,
        vendor_id,
        device_id);

    int errno;

    usb_hc_t* hc = zalloc(usb_hc_t);

    if (unlikely(!hc))
    {
        log_warning("[%s %04x:%04x] no memory",
            usb_hc_type_string(hc_driver->type),
            pci_device->vendor_id,
            pci_device->device_id);
        return;
    }

    hc->type = type;
    hc->id = hc_driver->next_hc_id++;
    hc->ops = hc_driver->ops;
    list_init(&hc->list_entry);

    if (unlikely(errno = hc_driver->ops->init(hc, pci_device)))
    {
        log_info("[%s %04x:%04x] initialization failed: %s",
            usb_hc_type_string(hc_driver->type),
            pci_device->vendor_id,
            pci_device->device_id,
            errno_name(errno));
        delete(hc);
        return;
    }

    list_add_tail(&hc->list_entry, &hcs);
}

static void usb_hc_init_type(int type)
{
    usb_hc_driver_t* hc_driver;
    if ((hc_driver = usb_hc_driver_find(type)))
    {
        pci_device_enumerate(&usb_hc_init_single, hc_driver);
    }
}

static void usb_hc_poll(void*)
{
    usb_hc_t* hc;
    timeval_t tv = {.tv_usec = 500000};

    REPEAT_PER(tv)
    {
        list_for_each_entry(hc, &hcs, list_entry)
        {
            hc->ops->ports_poll(hc);
        }
    }
}

UNMAP_AFTER_INIT int usb_hc_init(void)
{
    usb_hc_init_type(PCI_USB_xHCI);
    usb_hc_init_type(PCI_USB_EHCI);
    usb_hc_init_type(PCI_USB_UHCI);
    usb_hc_init_type(PCI_USB_OHCI);

    if (!list_empty(&hcs))
    {
        process_spawn("kusb-poll", &usb_hc_poll, NULL, SPAWN_KERNEL);
    }

    return 0;
}

UNMAP_AFTER_INIT int usb_hc_driver_register(usb_hc_ops_t* ops, int type)
{
    if (unlikely(!ops || !ops->init))
    {
        return -EINVAL;
    }

    usb_hc_driver_t* driver = zalloc(usb_hc_driver_t);

    if (unlikely(!driver))
    {
        return -ENOMEM;
    }

    list_init(&driver->list_entry);

    driver->type = type;
    driver->ops  = ops;

    list_add_tail(&driver->list_entry, &hc_drivers);

    log_info("registered %s driver", usb_hc_type_string(type));

    return 0;
}

int usb_hc_deinit(void)
{
    return 0;
}
