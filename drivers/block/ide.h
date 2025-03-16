#pragma once

#include <stdint.h>
#include <kernel/mbr.h>
#include <kernel/wait.h>
#include <kernel/mutex.h>
#include <kernel/kernel.h>

#include "ata.h"

// References:
// http://ftp.parisc-linux.org/docs/chips/PC87415.pdf
// https://pdos.csail.mit.edu/6.828/2018/readings/hardware/IDE-BusMaster.pdf
// https://wiki.osdev.org/PCI_IDE_Controller
// http://bos.asmhackers.net/docs/ata/docs/29860001.pdf
// https://forum.osdev.org/viewtopic.php?f=1&t=21151
// https://bochs.sourceforge.io/techspec/IDE-reference.txt

typedef struct request request_t;
typedef struct ide_device ide_device_t;
typedef struct ide_channel ide_channel_t;

#define ATA_MASTER          0x00
#define ATA_SLAVE           0x01

#define ATA_REG_BASE        0x1f0
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
#define ATA_CHANNELS_SIZE   2
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

enum ide_polling_result
{
    IDE_NO_ERROR = 0,
    IDE_ERROR    = 1,
    IDE_TIMEOUT  = 2,
};

struct request
{
    ata_device_t* device;
    int           direction;
    size_t        offset;
    uint8_t       sectors;
    size_t        count;
    void*         buffer;
    volatile int  errno;
    int           dma;
};

struct ide_channel
{
    uint16_t          base;
    uint16_t          ctrl;
    uint16_t          bmide;
    mutex_t           lock;
    request_t*        current_request;
    wait_queue_head_t queue;
};
