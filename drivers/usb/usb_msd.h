#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/scsi.h>
#include <kernel/compiler.h>
#include <kernel/page_types.h>

#include "mem_pool.h"
#include "usb_device.h"
#include "usb_descriptors.h"

#define CBW_SIGNATURE 0x43425355
#define CSW_SIGNATURE 0x53425355

#define DMA_BLOCK_SIZE 32

struct usb_msd
{
    uint32_t        last_tag;
    usb_endpoint_t* in;
    usb_endpoint_t* out;
    size_t          size;
    size_t          block_size;
    uint8_t         max_lun;
    mem_pool_t*     dma_pool;
    scsi_inquiry_t  inquiry;
};

typedef struct usb_msd usb_msd_t;

#define RESET_SETUP() \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x21, \
            .bRequest      = 0xff, \
            .wValue        = 0x00, \
            .wIndex        = 0x00, \
            .wLength       = 0x00, \
        }; \
    })

#define CLEAR_ENDPOINT_HALT_SETUP(ep) \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x02, \
            .bRequest      = 0x01, \
            .wValue        = 0x00, \
            .wIndex        = ep, \
            .wLength       = 0x00, \
        }; \
    })

#define GET_MAX_LUN_SETUP() \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0xa1, \
            .bRequest      = 0xfe, \
            .wValue        = 0x00, \
            .wIndex        = 0x00, \
            .wLength       = 0x01, \
        }; \
    })

struct usb_cbw
{
    uint32_t signature;
    uint32_t tag;
    uint32_t len;
    uint8_t  direction;
    uint8_t  unit;
    uint8_t  cmd_len;
    uint8_t  cmd_data[16];
} PACKED;

typedef struct usb_cbw usb_cbw_t;

struct usb_csw
{
    uint32_t signature;
    uint32_t tag;
    uint32_t residue;
    uint8_t  status;
} PACKED;

typedef struct usb_csw usb_csw_t;
