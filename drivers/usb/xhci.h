#pragma once

#include <arch/io.h>
#include <kernel/page_types.h>
#include "usb.h"

enum
{
    CAPLENGTH     = 0x00,
    HCIVERSION    = 0x02,
    HCSPARAMS1    = 0x04,
    HCSPARAMS2    = 0x08,
    HCSPARAMS3    = 0x0c,
    HCCPARAMS1    = 0x10,
    DBOFF         = 0x14,
    RTSOFF        = 0x18,
    HCCPARAMS2    = 0x1c,

    USBCMD        = 0x00,
    USBCMD_RUN    = (1 << 0),
    USBCMD_HCRST  = (1 << 1),
    USBCMD_INTE   = (1 << 2),
    USBCMD_HSEE   = (1 << 3),
    USBCMD_LHCRST = (1 << 7),
    USBCMD_CSS    = (1 << 8),
    USBCMD_CRS    = (1 << 9),
    USBCMD_EWE    = (1 << 10),
    USBCMD_EU3S   = (1 << 11),
    USBCMD_CME    = (1 << 13),
    USBCMD_ETE    = (1 << 14),
    USBCMD_TSC_EN = (1 << 15),
    USBCMD_VTIOE  = (1 << 16),

    USBSTS        = 0x04,
    USBSTS_HCH    = (1 << 0),
    USBSTS_HSE    = (1 << 2),
    USBSTS_EINT   = (1 << 3),
    USBSTS_PCD    = (1 << 4),
    USBSTS_SSS    = (1 << 8),
    USBSTS_RSS    = (1 << 9),
    USBSTS_SRE    = (1 << 10),
    USBSTS_CNR    = (1 << 11),
    USBSTS_HCE    = (1 << 12),

    PAGESIZE      = 0x08,
    DNCTRL        = 0x14,
    CRCR          = 0x18,
    DCBAAP        = 0x30,
    CONFIG        = 0x38,

    PORTSC        = 0x00,
    PORTPMSC      = 0x04,
    PORTLI        = 0x08,
};

typedef struct trb trb_t;
typedef struct raw_trb raw_trb_t;

enum
{
    TRB_C   = (1 << 0),
    TRB_ENT = (1 << 1),
    TRB_ISP = (1 << 2),
    TRB_NS  = (1 << 3),
    TRB_CH  = (1 << 4),
    TRB_IOC = (1 << 5),
    TRB_IDT = (1 << 6),

    TRB_TYPE_NORMAL = 1 << 10,
    TRB_TYPE_SETUP  = 2 << 10,
    TRB_TYPE_DATA   = 3 << 10,
    TRB_TYPE_STATUS = 4 << 10,
    TRB_TYPE_ISOCH  = 5 << 10,
    TRB_TYPE_LINK   = 6 << 10,
    TRB_TYPE_EVDATA = 7 << 10,
    TRB_TYPE_NOP    = 8 << 10,

    TRB_DIR_IN = (1 << 16),
    TRB_DIR_OUT = (0 << 16),
};

struct raw_trb
{
    union
    {
        usb_setup_t setup;
        uint64_t    buffer_addr;
    };
    uint32_t len:17;
    uint32_t td_size:5;
    uint32_t interrupter:10;
    uint16_t status;
    uint16_t reserved;
};

struct trb
{
    raw_trb_t* raw_trb;
    uintptr_t  paddr;
    trb_t*     next;
    trb_t*     prev;
};

struct completion_trb
{
    uint64_t tbr_ptr;
    uint32_t param:24;
    uint32_t code:8;
    uint32_t c:1;
    uint32_t reserved:9;
    uint32_t type:6;
    uint32_t vf_id:8;
    uint32_t slot_id:8;
};

typedef struct completion_trb completion_trb_t;

struct event_ring_entry
{
    uint64_t addr;
    uint16_t size;
    uint16_t reserved[3];
};

typedef struct event_ring_entry event_ring_entry_t;

#define RING_SIZE 64

struct ring
{
    raw_trb_t raw_trbs[RING_SIZE];
    trb_t     trbs[RING_SIZE];
    trb_t*    enqueue_ptr;
    trb_t*    dequeue_ptr;
    uint8_t   cycle_state;
};

typedef struct ring ring_t;

struct xhci
{
    void*   cap_base;
    void*   oper_base;
    void*   db_base;
    void*   rt_base;
    page_t* cmd_ring_page;
    ring_t* cmd_ring;

    page_t* event_ring_page;
    ring_t* event_ring;

    page_t* transfer_ring_page;
    ring_t* transfer_ring;
};

typedef struct xhci xhci_t;

static inline uint8_t xhci_caplength_read(xhci_t* xhci)
{
    return readb(xhci->cap_base + CAPLENGTH);
}

static inline uint16_t xhci_hciversion_read(xhci_t* xhci)
{
    return readw(xhci->cap_base + HCIVERSION);
}

static inline uint32_t xhci_hcsparams1_read(xhci_t* xhci)
{
    return readl(xhci->cap_base + HCSPARAMS1);
}

static inline uint32_t xhci_hcsparams2_read(xhci_t* xhci)
{
    return readl(xhci->cap_base + HCSPARAMS2);
}

static inline uint32_t xhci_hcsparams3_read(xhci_t* xhci)
{
    return readl(xhci->cap_base + HCSPARAMS3);
}

static inline uint32_t xhci_hccparams1_read(xhci_t* xhci)
{
    return readl(xhci->cap_base + HCCPARAMS1);
}

static inline uint32_t xhci_dboff_read(xhci_t* xhci)
{
    return readl(xhci->cap_base + DBOFF);
}

static inline uint32_t xhci_rtsoff_read(xhci_t* xhci)
{
    return readl(xhci->cap_base + RTSOFF);
}

static inline uint32_t xhci_hccparams2_read(xhci_t* xhci)
{
    return readl(xhci->cap_base + HCCPARAMS2);
}

static inline uint32_t xhci_usbcmd_read(xhci_t* xhci)
{
    return readl(xhci->oper_base + USBCMD);
}

static inline void xhci_usbcmd_write(xhci_t* xhci, uint32_t value)
{
    writel(value, xhci->oper_base + USBCMD);
}

static inline uint32_t xhci_usbsts_read(xhci_t* xhci)
{
    return readl(xhci->oper_base + USBSTS);
}

static inline void xhci_usbsts_write(xhci_t* xhci, uint32_t value)
{
    writel(value, xhci->oper_base + USBSTS);
}

static inline uint32_t xhci_pagesize_read(xhci_t* xhci)
{
    return readl(xhci->oper_base + PAGESIZE);
}

static inline void xhci_crcr_write(xhci_t* xhci, uint64_t value)
{
    writeq(value, xhci->oper_base + CRCR);
}

static inline void xhci_dcbaap_write(xhci_t* xhci, uint32_t value)
{
    writel(value, xhci->oper_base + DCBAAP);
}

static inline void xhci_config_write(xhci_t* xhci, uint32_t value)
{
    writel(value, xhci->oper_base + CONFIG);
}
