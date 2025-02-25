#pragma once

#include <stddef.h>
#include <arch/pci.h>
#include <kernel/list.h>

typedef struct usb_hc usb_hc_t;
typedef struct usb_device usb_device_t;
typedef struct usb_packet usb_packet_t;
typedef struct usb_endpoint usb_endpoint_t;

struct usb_hc_ops
{
    const char* name;
    int (*init)(usb_hc_t* hc, pci_device_t* pci_device);
    void (*ports_poll)(usb_hc_t* hc);
    int (*address_assign)(usb_device_t* device);
    int (*control_transfer)(usb_device_t* device, usb_packet_t* packets, size_t count);
    int (*bulk_transfer)(usb_device_t* device, usb_endpoint_t* endpoint, void* data, size_t size);
};

typedef struct usb_hc_ops usb_hc_ops_t;

struct usb_hc
{
    int           type;
    int           id;
    usb_hc_ops_t* ops;
    void*         data;
    list_head_t   list_entry;
};

int usb_hc_driver_register(usb_hc_ops_t* ops, int type);
