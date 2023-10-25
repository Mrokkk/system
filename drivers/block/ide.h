#pragma once

#include <stdint.h>
#include <kernel/mbr.h>
#include <kernel/kernel.h>
#include "ata.h"

struct ide_device;
struct ide_channel;

typedef struct ide_device ide_device_t;
typedef struct ide_channel ide_channel_t;

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

#define ATAPI_CMD_READ          0xa8
#define ATAPI_CMD_EJECT         0x1b

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA             0x00
#define IDE_ATAPI           0x01

#define ATA_MASTER          0x00
#define ATA_SLAVE           0x01

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT0   0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_COMMAND     0x07
#define ATA_REG_STATUS      0x07
#define ATA_REG_SECCOUNT1   0x02
#define ATA_REG_LBA3        0x03
#define ATA_REG_LBA4        0x04
#define ATA_REG_LBA5        0x05
#define ATA_REG_CONTROL     0x00
#define ATA_REG_ALTSTATUS   0x00

// Channels
#define ATA_PRIMARY         0x00
#define ATA_SECONDARY       0x01

// Directions
#define ATA_READ            0x00
#define ATA_WRITE           0x01

#define ATA_SECTOR_SIZE     512

#define BM_CMD_START        (1 << 0)
#define BM_CMD_READ         (1 << 3)
#define BM_CMD_WRITE        (0 << 3)

#define BM_STATUS_ACTIVE    (1 << 0)
#define BM_STATUS_ERROR     (1 << 1)
#define BM_STATUS_INTERRUPT (1 << 2)
#define BM_STATUS_DRV0_DMA  (1 << 5)
#define BM_STATUS_DRV1_DMA  (1 << 6)
#define BM_STATUS_SIMPLEX   (1 << 7)

struct ide_channel
{
    uint16_t base;
    uint16_t data_reg;
    uint16_t error_reg;
    uint16_t nsector_reg;
    uint16_t sector_reg;
    uint16_t lcyl_reg;
    uint16_t hcyl_reg;
    uint16_t select_reg;
    uint16_t status_reg;
    uint16_t ctrl;
    uint16_t irq_reg;
    uint16_t bmide;
};

struct ide_device
{
    uint16_t signature;      // Drive Signature
    uint8_t dma:1;
    uint8_t  channel:1;      // 0 (Primary Channel) or 1 (Secondary Channel).
    uint8_t  drive:1;        // 0 (Master Drive) or 1 (Slave Drive).
    uint8_t  type:1;         // 0: ATA, 1:ATAPI.
    uint16_t capabilities;   // Features.
    uint32_t command_sets;   // Command Sets Supported.
    uint32_t size;           // Size in Sectors.
    uint8_t  model[41];      // model in string.
    ide_channel_t* channel2;
    mbr_t* mbr;
};
