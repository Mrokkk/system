#pragma once

#include <stdint.h>
#include <kernel/mbr.h>
#include <kernel/kernel.h>

// References:
// https://people.freebsd.org/~imp/asiabsdcon2015/works/d2161r5-ATAATAPI_Command_Set_-_3.pdf
// http://users.utcluj.ro/~baruch/media/siee/labor/ATA-Interface.pdf

typedef struct ata_device ata_device_t;

#define ATA_SR_BSY              0x80    // Busy
#define ATA_SR_DRDY             0x40    // Drive ready
#define ATA_SR_DF               0x20    // Drive write fault
#define ATA_SR_DSC              0x10    // Drive seek complete
#define ATA_SR_DRQ              0x08    // Data request ready
#define ATA_SR_CORR             0x04    // Corrected data
#define ATA_SR_IDX              0x02    // Index
#define ATA_SR_ERR              0x01    // Error

#define ATA_ER_BBK              0x80    // Bad block
#define ATA_ER_UNC              0x40    // Uncorrectable data
#define ATA_ER_MC               0x20    // Media changed
#define ATA_ER_IDNF             0x10    // ID mark not found
#define ATA_ER_MCR              0x08    // Media change request
#define ATA_ER_ABRT             0x04    // Command aborted
#define ATA_ER_TK0NF            0x02    // Track 0 not found
#define ATA_ER_AMNF             0x01    // No address mark

#define ATA_IDENT_SIZE          128
#define ATA_IDENT_DEVICETYPE    0
#define ATA_IDENT_CYLINDERS     2
#define ATA_IDENT_HEADS         6
#define ATA_IDENT_SECTORS       12
#define ATA_IDENT_SERIAL        20
#define ATA_IDENT_MODEL         54
#define ATA_IDENT_CAPABILITIES  98
#define ATA_IDENT_FIELDVALID    106
#define ATA_IDENT_MAX_LBA       120
#define ATA_IDENT_COMMANDSETS   164
#define ATA_IDENT_MAX_LBA_EXT   200

#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_READ_DMA        0xc8
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_WRITE_DMA       0xca
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_CACHE_FLUSH     0xe7
#define ATA_CMD_CACHE_FLUSH_EXT 0xea
#define ATA_CMD_PACKET          0xa0
#define ATA_CMD_IDENTIFY_PACKET 0xa1
#define ATA_CMD_IDENTIFY        0xec

#define ATA_SECTOR_SIZE         512
#define ATAPI_SECTOR_SIZE       2048

#define ATA_TYPE_ATA    0
#define ATA_TYPE_ATAPI  1

struct ata_device
{
    uint16_t signature;      // Drive Signature
    uint8_t  id;
    uint8_t  type:1;         // 0: ATA, 1:ATAPI.
    uint8_t  dma:1;
    uint16_t capabilities;   // Features.
    uint32_t command_sets;   // Command Sets Supported.
    uint32_t size;           // Size in Sectors.
    char     model[41];      // model in string.
    void*    data;
    mbr_t mbr;
};

void ata_device_initialize(ata_device_t* device, uint8_t* buf, uint8_t id, void* data);

static inline void ata_device_print(ata_device_t* device)
{
    const char* unit;
    uint64_t size = (uint64_t)device->size * ATA_SECTOR_SIZE;

    human_size(size, unit);

    log_info("%u: %s drive %u %s: %s; signature: %x",
        device->id,
        device->type ? "ATAPI" : "ATA",
        (uint32_t)size,
        unit,
        device->model,
        device->signature);

    log_continue("; cap: %x", device->capabilities);
    if (device->dma)
    {
        log_continue("; DMA");
    }
    if (device->capabilities & 0x200)
    {
        log_continue("; LBA");
    }
    else
    {
        log_continue("; CHS");
    }
}
