#pragma once

#include <stdint.h>
#include <kernel/mbr.h>
#include <kernel/kernel.h>

typedef struct request request_t;
typedef struct ide_device ide_device_t;
typedef struct ide_channel ide_channel_t;

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

#define BM_REG_CMD          0
#define BM_REG_STATUS       2
#define BM_REG_PRDT         4

#define BM_CMD_START        (1 << 0)
#define BM_CMD_READ         (1 << 3)
#define BM_CMD_WRITE        (0 << 3)

#define BM_STATUS_ACTIVE    (1 << 0)
#define BM_STATUS_ERROR     (1 << 1)
#define BM_STATUS_INTERRUPT (1 << 2)
#define BM_STATUS_DRV0_DMA  (1 << 5)
#define BM_STATUS_DRV1_DMA  (1 << 6)
#define BM_STATUS_SIMPLEX   (1 << 7)

struct request
{
    int drive;
    int direction;
    uint32_t offset;
    uint8_t sectors;
    uint32_t count;
    char* buffer;
    int errno;
    int dma;
};

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
