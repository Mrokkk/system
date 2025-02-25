#pragma once

#include <kernel/list.h>
#include "usb_device.h"

typedef struct usb_driver usb_driver_t;
typedef struct usb_driver_ops usb_driver_ops_t;

struct usb_driver_ops
{
    const char* name;
    int (*probe)(usb_device_t* device);
    int (*initialize)(usb_driver_t* driver);
    int (*release)(usb_driver_t* driver);
    list_head_t list_entry;
};

struct usb_driver
{
    usb_device_t*     device;
    usb_driver_ops_t* ops;
    void*             data;
};

int usb_driver_register(usb_driver_ops_t* ops);
