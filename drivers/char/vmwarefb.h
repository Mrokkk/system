#pragma once

#include <stddef.h>
#include <stdint.h>

#define PCI_VENDOR_ID_VMWARE       0x15ad
#define PCI_DEVICE_ID_VMWARE_SVGA2 0x0405

#define SVGA_MAGIC         0x900000UL
#define SVGA_MAKE_ID(ver)  (SVGA_MAGIC << 8 | (ver))

#define SVGA_VERSION_2     2
#define SVGA_ID_2          SVGA_MAKE_ID(SVGA_VERSION_2)

#define SVGA_INDEX            0
#define SVGA_VALUE            1
#define SVGA_BIOS             2
#define SVGA_IRQSTATUS        8

#define SVGA_REG_ID           0
#define SVGA_REG_ENABLE       1
#define SVGA_REG_WIDTH        2
#define SVGA_REG_HEIGHT       3
#define SVGA_REG_MAX_WIDTH    4
#define SVGA_REG_MAX_HEIGHT   5
#define SVGA_REG_BPP          7
#define SVGA_REG_FB_START     13
#define SVGA_REG_FB_OFFSET    14
#define SVGA_REG_VRAM_SIZE    15
#define SVGA_REG_FB_SIZE      16
#define SVGA_REG_CAPABILITIES 17
#define SVGA_REG_FIFO_START   18
#define SVGA_REG_FIFO_SIZE    19
#define SVGA_REG_CONFIG_DONE  20
#define SVGA_REG_SYNC         21
#define SVGA_REG_BUSY         22
#define SVGA_REG_HOST_BPP     28

#define SVGA_FIFO_MIN         0
#define SVGA_FIFO_MAX         1
#define SVGA_FIFO_NEXT_CMD    2
#define SVGA_FIFO_STOP        3

#define SVGA_CMD_UPDATE       1

#define SVGA_CAP_8BIT_EMULATION 0x00000100

struct svga_fifo
{
    uint32_t start;
    uint32_t size;
    uint32_t next_command;
    uint32_t stop;
    uint32_t commands[];
};

typedef struct svga_fifo svga_fifo_t;

struct svga
{
    uint16_t     iobase;
    uintptr_t    fb_paddr;
    size_t       fb_offset;
    size_t       fb_size;
    void*        fb_vaddr;
    uint32_t     fifo_paddr;
    size_t       fifo_size;
    uint32_t     resx, resy;
    uint32_t     bpp;
    uint32_t     cap;
    svga_fifo_t* fifo;
};

typedef struct svga svga_t;
