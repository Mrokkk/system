#pragma once

#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/mutex.h>
#include "mem_pool.h"

enum
{
    USBCMD                           = 0x00,
    USBCMD_RUN                       = (1 << 0),
    USBCMD_HOST_CONTROLLER_RESET     = (1 << 1),
    USBCMD_GLOBAL_RESET              = (1 << 2),
    USBCMD_ENTER_GLOBAL_SUSPEND_MODE = (1 << 3),
    USBCMD_FORCE_GLOBAL_RESUME       = (1 << 4),
    USBCMD_SOFTWARE_DEBUG            = (1 << 5),
    USBCMD_CONFIGURE_FLAG            = (1 << 6),
    USBCMD_MAX_PACKET                = (1 << 7),

    USBSTS                               = 0x02,
    USBSTS_USB_INTERRUPT                 = (1 << 0),
    USBSTS_USB_ERROR_INTERRUPT           = (1 << 1),
    USBSTS_RESUME_RECEIVED               = (1 << 2),
    USBSTS_HOST_SYSTEM_ERROR             = (1 << 3),
    USBSTS_HOST_CONTROLLER_PROCESS_ERROR = (1 << 4),
    USBSTS_HOST_CONTROLLER_HALTED        = (1 << 5),
    USBSTS_ERROR                         =
        USBSTS_USB_ERROR_INTERRUPT |
        USBSTS_HOST_SYSTEM_ERROR |
        USBSTS_HOST_CONTROLLER_HALTED |
        USBSTS_HOST_CONTROLLER_PROCESS_ERROR,

    USBINTR                          = 0x04,
    USBINTR_TIMEOUT_CRC_ENABLE       = (1 << 0),
    USBINTR_RESUME_INTR_ENABLE       = (1 << 1),
    USBINTR_IOC_ENABLE               = (1 << 2),
    USBINTR_SHORT_PACKET_INTR_ENABLE = (1 << 3),

    FRNUM     = 0x06,
    FRBASEADD = 0x08,
    SOFMOD    = 0x0c,
    PORTSC1   = 0x10,
    PORTSC2   = 0x12,

    PORTSC_PRESENT       = (1 << 0),
    PORTSC_CHANGE        = (1 << 1),
    PORTSC_ENABLE        = (1 << 2),
    PORTSC_ENABLE_CHANGE = (1 << 3),
    PORTSC_VALID         = (1 << 7),
    PORTSC_LOW_SPEED     = (1 << 8),
    PORTSC_RESET         = (1 << 9),

    FRLIST_SIZE      = 1024,
    FRLIST_SELECT_QH = (1 << 1),
    FRLIST_SELECT_TD = (0 << 1),

    TD_STATUS_BIT_STUFF_ERROR = (1 << 17),
    TD_STATUS_CRC_TIMEOUT     = (1 << 18),
    TD_STATUS_NAK             = (1 << 19),
    TD_STATUS_BABBLE          = (1 << 20),
    TD_STATUS_BUFFER_ERROR    = (1 << 21),
    TD_STATUS_STALLED         = (1 << 22),
    TD_STATUS_ACTIVE          = (1 << 23),
    TD_STATUS_ERROR           =
        TD_STATUS_STALLED |
        TD_STATUS_BUFFER_ERROR |
        TD_STATUS_BABBLE |
        TD_STATUS_NAK |
        TD_STATUS_CRC_TIMEOUT |
        TD_STATUS_BIT_STUFF_ERROR,
    TD_STATUS_IOC             = (1 << 24),
    TD_STATUS_LOW_SPEED       = (1 << 26),

    TD_NEXT_DEPTH_FIRST = (1 << 2),
    TD_TOKEN_TOGGLE     = (1 << 19),

    TERMINATE = (1 << 0),

    UHCI_MAX_PORTS_COUNT = 8,

    USB_LEGKEY = 0xc0,
    USBPIRQEN = (1 << 13),
};

#define TD_TOKEN_DEVICE_ADDR_SET(addr)   ((addr) << 8)
#define TD_TOKEN_ENDPOINT_ADDR_SET(addr) ((addr) << 15)
#define TD_TOKEN_TOGGLE_SET(toggle)      ((toggle) << 19)
#define TD_TOKEN_SIZE_SET(size)          (((((uint32_t)size) - 1) & 0x7ff) << 21)

struct uhci_td
{
    io32         next;
    io32         status;
    io32         token;
    io32         buffer_addr;
    uint32_t     paddr;
    uint32_t     padding[11];
};

typedef struct uhci_td uhci_td_t;

struct uhci_qh
{
    io32     next;
    io32     first;
    uint32_t paddr;
    uint32_t padding[5];
};

typedef struct uhci_qh uhci_qh_t;

struct uhci
{
    pci_device_t* pci;
    uint16_t      iobase;
    uint8_t       ports;
    mutex_t       lock;
    page_t*       frame_list_page;
    io32*         frame_list;
    mem_pool_t*   td_pool;
    mem_pool_t*   qh_pool;
    uhci_qh_t*    skel_qh;
    int           last_addr;
    int           hc_id;
};

typedef struct uhci uhci_t;

#define DEFINE_REGISTER_RW_FUNC(reg, name, type, suffix) \
    static inline type uhci_##name##_read(uhci_t* uhci) \
    { \
        return in##suffix(uhci->iobase + reg); \
    } \
    static inline void uhci_##name##_write(uhci_t* uhci, type value) \
    { \
        out##suffix(value, uhci->iobase + reg); \
    }

DEFINE_REGISTER_RW_FUNC(USBCMD, usbcmd, uint16_t, w);
DEFINE_REGISTER_RW_FUNC(USBSTS, usbsts, uint16_t, w);
DEFINE_REGISTER_RW_FUNC(USBINTR, usbintr, uint16_t, w);
DEFINE_REGISTER_RW_FUNC(FRNUM, frnum, uint16_t, w);
DEFINE_REGISTER_RW_FUNC(SOFMOD, sofmod, uint8_t, b);
DEFINE_REGISTER_RW_FUNC(FRBASEADD, frbaseadd, uint32_t, l);

static inline uint16_t uhci_portsc_read(uhci_t* uhci, int port)
{
    return inw(uhci->iobase + PORTSC1 + port * 2);
}

static inline void uhci_portsc_write(uhci_t* uhci, int port, uint16_t value)
{
    outw(value, uhci->iobase + PORTSC1 + port * 2);
}
