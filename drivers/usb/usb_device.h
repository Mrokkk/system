#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <kernel/list.h>
#include <kernel/page_types.h>

#include "usb.h"
#include "usb_hc.h"
#include "usb_descriptors.h"

typedef struct usb_device usb_device_t;
typedef struct usb_driver usb_driver_t;

enum usb_speed
{
    USB_SPEED_LOW,
    USB_SPEED_FULL,
    USB_SPEED_HIGH,
};

typedef enum usb_speed usb_speed_t;

struct usb_transfer
{
    void*                data;
    size_t               size;
    usb_endpoint_desc_t* endpoint;
};

typedef struct usb_transfer usb_transfer_t;

struct usb_endpoint
{
    usb_endpoint_desc_t desc;
    uint8_t             toggle;
};

typedef struct usb_endpoint usb_endpoint_t;

struct usb_interface
{
    usb_interface_desc_t desc;
    size_t               num_endpoints;
    usb_endpoint_t*      endpoints;
};

typedef struct usb_interface usb_interface_t;

struct usb_configuration
{
    usb_configuration_desc_t desc;
    size_t                   num_interfaces;
    usb_interface_t*         interfaces;
};

typedef struct usb_configuration usb_configuration_t;

#define USB_TRANSFER(...) \
    ({ \
        (usb_transfer_t){ \
            __VA_ARGS__ \
        }; \
    })

struct usb_packet
{
    int       type;
    size_t    len;
    page_t*   data_page;
    uintptr_t data_paddr;
};

typedef struct usb_packet usb_packet_t;

struct usb_device
{
    char                 id[64];
    void*                hc;
    usb_hc_ops_t*        ops;
    int                  controller_id;
    uint16_t             class;
    uint8_t              port;
    uint8_t              address;
    usb_speed_t          speed;
    int                  langid;
    usb_device_desc_t    desc;
    size_t               num_configurations;
    usb_configuration_t* configurations;
    usb_driver_t*        driver;
    list_head_t          list_entry;
};

int usb_device_detect(void* controller, usb_hc_ops_t* ops, int controller_id, int port, usb_speed_t speed);
int usb_control_transfer(usb_device_t* device, usb_setup_t packet, void* data);
void usb_controller_port_disconnected(void* controller, int port);
int usb_device_configuration_set(usb_device_t* device, usb_configuration_t* config);
