#include "ata.h"

#define identb(offset) *((uint8_t*)(buf + (offset)))
#define identw(offset) *((uint16_t*)(buf + (offset)))
#define identl(offset) *((uint32_t*)(buf + (offset)))

void ata_device_initialize(ata_device_t* device, uint8_t* buf, uint8_t id, void* data)
{
    int i;

    device->id           = id;
    device->signature    = identw(ATA_IDENT_DEVICETYPE);
    device->capabilities = identw(ATA_IDENT_CAPABILITIES);
    device->command_sets = identl(ATA_IDENT_COMMANDSETS);
    device->data         = data;
    device->type         = (device->signature >> 15)
        ? ATA_TYPE_ATAPI
        : ATA_TYPE_ATA;

    if (device->type == ATA_TYPE_ATA)
    {
        if (device->command_sets & (1 << 26))
        {
            // Device uses 48-Bit Addressing:
            device->sectors = identl(ATA_IDENT_MAX_LBA_EXT);
        }
        else
        {
            // Device uses CHS or 28-bit Addressing:
            device->sectors = identl(ATA_IDENT_MAX_LBA);
        }

        if (!device->sectors)
        {
            // Fallback to CHS
            device->sectors = identw(ATA_IDENT_CYLINDERS) * identw(ATA_IDENT_HEADS) * identw(ATA_IDENT_SECTORS);
        }

        uint16_t word106 = identw(212);

        if ((word106 & (1 << 14)) && !(word106 & (1 << 15)))
        {
            if (word106 & (1 << 12))
            {
                device->sector_size = identl(ATA_IDENT_SECTOR_SIZE);
            }
        }

        if (!device->sector_size)
        {
            device->sector_size = ATA_SECTOR_SIZE;
        }
    }
    else
    {
        device->sectors     = 0;
        device->sector_size = 0;
    }

    for (i = 0; i < 40; i += 2)
    {
        if (identb(ATA_IDENT_MODEL + i + 1) < 0x20 || identb(ATA_IDENT_MODEL + i + 1) > 0x7e)
        {
            snprintf(device->model, sizeof(device->model), "<empty>");
        }
        else
        {
            device->model[i] = identb(ATA_IDENT_MODEL + i + 1);
            device->model[i + 1] = identb(ATA_IDENT_MODEL + i);
        }
    }

    for (; i; --i)
    {
        if (device->model[i] != ' ' && device->model[i] != 0)
        {
            break;
        }
    }

    device->model[i + 1] = 0;
}
