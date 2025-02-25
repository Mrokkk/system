#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "usb_descriptors.h"

enum
{
    USB_PACKET_IN    = 0x69,
    USB_PACKET_OUT   = 0xe1,
    USB_PACKET_SETUP = 0x2d,

    USB_PACKET_H2D   = (0 << 7),
    USB_PACKET_D2H   = (1 << 7),

    USB_REQUEST_SET_ADDRESS       = 5,
    USB_REQUEST_GET_DESCRIPTOR    = 6,
    USB_REQUEST_GET_CONFIGURATION = 8,
    USB_REQUEST_SET_CONFIGURATION = 9,

    USB_DESC_DEVICE        = 1,
    USB_DESC_CONFIGURATION = 2,
    USB_DESC_STRING        = 3,
    USB_DESC_INTERFACE     = 4,
    USB_DESC_ENDPOINT      = 5,

    USB_CLASS_UNKNOWN  = 0,
    USB_CLASS_AUDIO    = 1,
    USB_CLASS_CDC      = 2,
    USB_CLASS_HID      = 3,
    USB_CLASS_STORAGE  = 8,
    USB_CLASS_HUB      = 9,

    USB_STRING_SIZE = 128,
};

enum pci_usb_prog_if
{
    PCI_USB_UHCI = 0x00,
    PCI_USB_OHCI = 0x10,
    PCI_USB_EHCI = 0x20,
    PCI_USB_xHCI = 0x30,
};

struct usb_setup
{
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

typedef struct usb_setup usb_setup_t;

#define GET_DESCRIPTOR_SETUP(desc, size) \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x80, \
            .bRequest      = USB_REQUEST_GET_DESCRIPTOR, \
            .wValue        = (desc) << 8, \
            .wIndex        = 0x00, \
            .wLength       = size, \
        }; \
    })

#define SET_ADDRESS_SETUP(addr) \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x00, \
            .bRequest      = USB_REQUEST_SET_ADDRESS, \
            .wValue        = addr, \
            .wIndex        = 0, \
            .wLength       = 0, \
        }; \
    })

#define GET_DESCRIPTOR_STRING_SETUP(id, langid, size) \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x80, \
            .bRequest      = USB_REQUEST_GET_DESCRIPTOR, \
            .wValue        = (USB_DESC_STRING << 8) | (id), \
            .wIndex        = langid, \
            .wLength       = size, \
        }; \
    })

#define GET_DESCRIPTOR_CONFIGURATION_SETUP(id, size) \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x80, \
            .bRequest      = USB_REQUEST_GET_DESCRIPTOR, \
            .wValue        = (USB_DESC_CONFIGURATION << 8) | (id), \
            .wIndex        = 0x00, \
            .wLength       = size, \
        }; \
    })

#define GET_DESCRIPTOR_INTERFACE_SETUP(id, size) \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x80, \
            .bRequest      = 0x06, \
            .wValue        = (USB_DESC_INTERFACE << 8) | (id), \
            .wIndex        = 0x00, \
            .wLength       = size, \
        }; \
    })

#define SET_CONFIGURATION_SETUP(id) \
    ({ \
        (usb_setup_t){ \
            .bmRequestType = 0x00, \
            .bRequest      = USB_REQUEST_SET_CONFIGURATION, \
            .wValue        = (id), \
            .wIndex        = 0x00, \
            .wLength       = 0x00, \
        }; \
    })
