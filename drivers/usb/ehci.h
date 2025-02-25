#pragma once

#include <arch/io.h>
#include <arch/pci.h>

#include "mem_pool.h"

enum
{
    USBLEGSUP_HC_BIOS_OWNERSHIP = (1 << 16),
    USBLEGSUP_HC_OS_OWNERSHIP   = (1 << 24),

    CAPLENGTH      = 0x00,
    HCIVERSION     = 0x02,
    HCSPARAMS      = 0x04,
    HCCPARAMS      = 0x08,
    HCSP_PORTROUTE = 0x0c,

    USBCMD                   = 0x00,
    USBCMD_RUN               = (1 << 0),
    USBCMD_HCRESET           = (1 << 1),
    USBCMD_FRAME_SIZE_1024   = (0 << 2),
    USBCMD_PERIODIC_SCHEDULE = (1 << 4),
    USBCMD_ASYNC_SCHEDULE    = (1 << 5),
    USBCMD_THRESHOLD_8MF     = (8 << 16),

    USBSTS                 = 0x04,
    USBSTS_INT             = (1 << 0),
    USBSTS_ERRINT          = (1 << 1),
    USBSTS_CHANGE          = (1 << 2),
    USBSTS_FRLIST_ROLLOVER = (1 << 3),
    USBSTS_HOST_ERROR      = (1 << 4),
    USBSTS_HCHALTED        = (1 << 12),
    USBSTS_ASYNC_STS       = (1 << 16),

    USBINTR          = 0x08,
    FRINDEX          = 0x0c,
    CTRLDSSEGMENT    = 0x10,
    PERIODICLISTBASE = 0x14,
    ASYNCLISTADDR    = 0x18,
    CONFIGFLAG       = 0x40,

    PORTSC         = 0x44,
    PORTSC_PRESENT = (1 << 0),
    PORTSC_CHANGE  = (1 << 1),
    PORTSC_ENABLE  = (1 << 2),
    PORTSC_RESET   = (1 << 8),
    PORTSC_OWNER   = (1 << 13),

    TERMINATE = 1,

    QTD_STS_PING   = (1 << 0),
    QTD_STS_STS    = (1 << 1),
    QTD_STS_MMF    = (1 << 2),
    QTD_STS_XACT   = (1 << 3),
    QTD_STS_BABBLE = (1 << 4),
    QTD_STS_DBE    = (1 << 5),
    QTD_STS_HALT   = (1 << 6),
    QTD_STS_ACTIVE = (1 << 7),
    QTD_PID_OUT    = (0 << 8),
    QTD_PID_IN     = (1 << 8),
    QTD_PID_SETUP  = (2 << 8),
    QTD_CERR_1     = (1 << 10),
    QTD_CERR_2     = (2 << 10),
    QTD_CERR_3     = (3 << 10),
    QTD_IOC        = (1 << 15),
    QTD_TOGGLE     = (1 << 31),
    QTD_STS_ERROR  =
        QTD_STS_HALT |
        QTD_STS_DBE |
        QTD_STS_BABBLE |
        QTD_STS_XACT,

    QH_NEXT_QH    = (1 << 1),

    QH_EP_CHAR_EPS_FULL = (0 << 12),
    QH_EP_CHAR_EPS_LOW  = (1 << 12),
    QH_EP_CHAR_EPS_HIGH = (2 << 12),
    QH_EP_CHAR_DTC      = (1 << 14),
    QH_EP_CHAR_H        = (1 << 15),
};

#define HCSPARAMS_N_PORTS(value)     (((value) >> 0) & 0xf)
#define HCSPARAMS_PPC(value)         (((value) >> 4) & 0x1)
#define HCSPARAMS_PRR(value)         (((value) >> 7) & 0x1)
#define HCSPARAMS_N_PCC(value)       (((value) >> 8) & 0xf)
#define HCSPARAMS_N_CC(value)        (((value) >> 12) & 0xf)
#define HCSPARAMS_P_INDICATOR(value) (((value) >> 16) & 0x1)
#define HCSPARAMS_DEBUG_PORT(value)  (((value) >> 20) & 0xf)

#define HCCPARAMS_64BIT(value)       (((value) >> 0) & 0x1)
#define HCCPARAMS_EECP_GET(value)    (((value) >> 8) & 0xff)

#define QTD_STS_GET(value)           ((value) & 0xff)
#define QTD_TOTAL_LEN_SET(len)       ((len) << 16)
#define QTD_TOTAL_LEN_GET(len)       (((len) >> 16) & 0x4fff)
#define QTD_TOGGLE_SET(value)        ((value) << 31)

#define QH_DEVICE_ADDRESS_SET(addr)  (addr)
#define QH_ENDPOINT_ADDR_SET(addr)   ((addr) << 8)
#define QH_MAX_PACKET_LEN_SET(len)   ((len) << 16)

typedef io32 ehci_frame_t;

struct ehci_qtd
{
    uint32_t next;
    uint32_t alt_next;
    io32     status;
    uint32_t buffer_addr[5];
    uint32_t buffer_hi_addr[5];
    uint32_t paddr;
    uint32_t padding[2];
};

typedef struct ehci_qtd ehci_qtd_t;

struct ehci_qh
{
    uint32_t   next;
    uint32_t   ep_char;
    uint32_t   ep_cap;
    uint32_t   current_qtd;
    volatile ehci_qtd_t overlay;
    uint32_t   paddr;
    uint8_t    padding[44];
};

typedef struct ehci_qh ehci_qh_t;

struct ehci
{
    int           hc_id;
    int           last_addr;
    uint32_t      eecp;
    pci_device_t* pci;
    void*         cap_base;
    void*         reg_base;
    uint8_t       ports;
    page_t*       periodic_list_page;
    ehci_frame_t* periodic_list;
    ehci_qh_t*    qh;
    mem_pool_t*   qtd_pool;
    mem_pool_t*   qh_pool;
};

typedef struct ehci ehci_t;

static inline uint32_t ehci_hciversion_read(ehci_t* ehci)
{
    return readl(ehci->cap_base + HCIVERSION);
}

static inline uint32_t ehci_hcsparams_read(ehci_t* ehci)
{
    return readl(ehci->cap_base + HCSPARAMS);
}

static inline uint32_t ehci_hccparams_read(ehci_t* ehci)
{
    return readl(ehci->cap_base + HCCPARAMS);
}

static inline uint32_t ehci_usbcmd_read(ehci_t* ehci)
{
    return readl(ehci->reg_base + USBCMD);
}

static inline void ehci_usbcmd_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + USBCMD);
}

static inline uint32_t ehci_usbsts_read(ehci_t* ehci)
{
    return readl(ehci->reg_base + USBSTS);
}

static inline void ehci_usbsts_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + USBSTS);
}

static inline uint32_t ehci_usbintr_read(ehci_t* ehci)
{
    return readl(ehci->reg_base + USBINTR);
}

static inline void ehci_usbintr_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + USBINTR);
}

static inline uint32_t ehci_frindex_read(ehci_t* ehci)
{
    return readl(ehci->reg_base + FRINDEX);
}

static inline void ehci_frindex_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + FRINDEX);
}

static inline void ehci_periodiclistbase_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + PERIODICLISTBASE);
}

static inline uint32_t ehci_asynclistaddr_read(ehci_t* ehci)
{
    return readl(ehci->reg_base + ASYNCLISTADDR);
}

static inline void ehci_asynclistaddr_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + ASYNCLISTADDR);
}

static inline void ehci_ctrldssegment_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + CTRLDSSEGMENT);
}

static inline void ehci_configflag_write(ehci_t* ehci, uint32_t value)
{
    writel(value, ehci->reg_base + CONFIGFLAG);
}

static inline uint32_t ehci_portsc_read(ehci_t* ehci, int port)
{
    return readl(ehci->reg_base + PORTSC + port * 4);
}

static inline void ehci_portsc_write(ehci_t* ehci, int port, uint32_t value)
{
    writel(value, ehci->reg_base + PORTSC + port * 4);
}
