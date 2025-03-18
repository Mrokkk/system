#define log_fmt(fmt) "agp: " fmt
#include <arch/agp.h>
#include <arch/pci.h>
#include <kernel/kernel.h>

enum
{
    AGP_STS_RATE1X = (1 << 0),
    AGP_STS_RATE2X = (1 << 1),
    AGP_STS_RATE4X = (1 << 2),
    AGP_STS_FW     = (1 << 4),
    AGP_STS_4G     = (1 << 5),
    AGP_STS_SBA    = (1 << 9),

    AGP_CMD_RATE1X     = (1 << 0),
    AGP_CMD_RATE2X     = (1 << 1),
    AGP_CMD_RATE4X     = (1 << 2),
    AGP_CMD_FW_ENABLE  = (1 << 4),
    AGP_CMD_4G         = (1 << 5),
    AGP_CMD_AGP_ENABLE = (1 << 8),
    AGP_CMD_SBA_ENABLE = (1 << 9),
};

UNMAP_AFTER_INIT static void agp_init_one(pci_device_t* device, void*)
{
    pci_agp_cap_t cap = {};

    for (uint8_t ptr = device->capabilities; ptr; ptr = cap.next)
    {
        if (unlikely(pci_config_read(device, ptr, &cap, sizeof(pci_cap_t))))
        {
            log_warning("cannot read CAP at %#x", ptr);
            continue;
        }

        if (cap.id == PCI_CAP_ID_AGP)
        {
            char* vendor_id;
            char* device_id;

            if (unlikely(pci_config_read(device, ptr, &cap.status, 8)))
            {
                log_warning("cannot read CAP at %#x", ptr);
                continue;
            }

            pci_device_describe(device, &vendor_id, &device_id);
            log_notice("%s %s", vendor_id, device_id);
            log_notice("  AGP %u.%u: %#x %#x", cap.major, cap.minor, cap.status, cap.command);

            return;
        }
    }
}

UNMAP_AFTER_INIT void agp_initialize(void)
{
    pci_device_enumerate(&agp_init_one, NULL);
}
