#define log_fmt(fmt) "agp: " fmt
#include <arch/agp.h>
#include <arch/pci.h>
#include <kernel/kernel.h>

// References:
// https://www.dcs.ed.ac.uk/home/ecole/agp/agp10.pdf
// http://esd.cs.ucr.edu/webres/agp20.pdf

enum
{
    AGP_STS_RATE1X     = (1 << 0),
    AGP_STS_RATE2X     = (1 << 1),
    AGP_STS_RATE4X     = (1 << 2),
    AGP_STS_FW         = (1 << 4),
    AGP_STS_4G         = (1 << 5),
    AGP_STS_SBA        = (1 << 9),

    AGP_CMD_RATE1X     = (1 << 0),
    AGP_CMD_RATE2X     = (1 << 1),
    AGP_CMD_RATE4X     = (1 << 2),
    AGP_CMD_FW_ENABLE  = (1 << 4),
    AGP_CMD_4G         = (1 << 5),
    AGP_CMD_AGP_ENABLE = (1 << 8),
    AGP_CMD_SBA_ENABLE = (1 << 9),
};

struct pci_agp_cap
{
    uint8_t  id;
    uint8_t  next;
    uint8_t  minor:4;
    uint8_t  major:4;
    uint8_t  reserved;
    uint32_t status;
    uint32_t command;
};

typedef struct pci_agp_cap pci_agp_cap_t;

struct agp_device
{
    pci_device_t* pci;
    uint8_t       command_ptr;
};

typedef struct agp_device agp_device_t;

struct agp_context
{
    agp_device_t* agp_bridge;
    agp_device_t  devices[16];
    size_t        count;
    uint32_t      command;
    uint8_t       major;
};

typedef struct agp_context agp_context_t;

UNMAP_AFTER_INIT static void agp_init_devices(agp_context_t* ctx)
{
    uint32_t command = ctx->command;

    for (size_t i = 0; i < ctx->count; ++i)
    {
        pci_config_write(ctx->devices[i].pci, ctx->devices[i].command_ptr, &command, 4);
    }
}

UNMAP_AFTER_INIT static void agp_check_one(pci_device_t* pci_device, void* data)
{
    int errno;
    pci_agp_cap_t cap = {};
    agp_context_t* ctx = data;

    int cap_ptr = pci_cap_find(pci_device, PCI_CAP_ID_AGP, &cap, sizeof(cap));

    if (unlikely(errno = errno_get(cap_ptr)))
    {
        return;
    }

    if (ctx->count >= array_size(ctx->devices))
    {
        return;
    }

    agp_device_t* device = &ctx->devices[ctx->count++];

    device->pci = pci_device;
    device->command_ptr = cap_ptr + offsetof(pci_agp_cap_t, command);

    ctx->major = cap.major > ctx->major ? cap.major : ctx->major;

    if (pci_device->class == PCI_BRIDGE)
    {
        ctx->agp_bridge = device;
    }

    char* vendor_id;
    char* device_id;

    pci_device_describe(pci_device, &vendor_id, &device_id);
    log_notice("%s %s", vendor_id, device_id);
    log_notice("  AGP %u.%u: %#x", cap.major, cap.minor, cap.status);

    // Clear bits related to unsupported features/rates
    if (!(cap.status & AGP_STS_RATE4X))
    {
        ctx->command &= ~AGP_CMD_RATE4X;
    }
    if (!(cap.status & AGP_STS_RATE2X))
    {
        ctx->command &= ~AGP_CMD_RATE2X;
    }
    if (!(cap.status & AGP_STS_RATE2X))
    {
        ctx->command &= ~AGP_CMD_RATE2X;
    }
    if (!(cap.status & AGP_STS_FW))
    {
        ctx->command &= ~AGP_CMD_FW_ENABLE;
    }
    if (!(cap.status & AGP_STS_SBA))
    {
        ctx->command &= ~AGP_CMD_SBA_ENABLE;
    }
}

UNMAP_AFTER_INIT void agp_initialize(void)
{
    agp_context_t ctx = {
        .command
            = AGP_CMD_RATE1X
            | AGP_CMD_RATE2X
            | AGP_CMD_RATE4X
            | AGP_CMD_SBA_ENABLE
            | AGP_CMD_FW_ENABLE
            | AGP_CMD_AGP_ENABLE
    };

    pci_device_enumerate(&agp_check_one, &ctx);

    if (!ctx.count)
    {
        return;
    }

    if (unlikely(!ctx.agp_bridge))
    {
        log_error("AGP bridge not found");
        return;
    }

    if (ctx.major > 2)
    {
        log_notice("AGP 3 not supported");
        return;
    }

    uint32_t tmp = ctx.command;
    ctx.command &= ~(AGP_CMD_RATE1X | AGP_CMD_RATE2X | AGP_CMD_RATE4X);

    // Select highest supported rate
    if (tmp & AGP_CMD_RATE4X)
    {
        ctx.command |= AGP_CMD_RATE4X;
    }
    else if (tmp & AGP_CMD_RATE2X)
    {
        ctx.command |= AGP_CMD_RATE2X;
    }
    else if (tmp & AGP_CMD_RATE1X)
    {
        ctx.command |= AGP_CMD_RATE1X;
    }

    log_notice("command to set: %#x", ctx.command);

    agp_init_devices(&ctx);
}
