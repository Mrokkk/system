#pragma once

#include <stdint.h>

#define TYPEDEF(name) typedef struct name name##_t

TYPEDEF(usb_desc_base);
TYPEDEF(usb_device_desc);
TYPEDEF(usb_string_desc);
TYPEDEF(usb_langid_desc);
TYPEDEF(usb_configuration_desc);
TYPEDEF(usb_interface_desc);
TYPEDEF(usb_endpoint_desc);

struct usb_desc_base
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;
};

struct usb_device_desc
{
    usb_desc_base_t base;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};

struct usb_string_desc
{
    usb_desc_base_t base;
    uint16_t bString[];
};

struct usb_langid_desc
{
    usb_desc_base_t base;
    uint16_t wLangID[];
};

struct usb_configuration_desc
{
    usb_desc_base_t base;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
};

struct usb_interface_desc
{
    usb_desc_base_t base;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
};

struct usb_endpoint_desc
{
    usb_desc_base_t base;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
};

#define USB_ENDPOINT_IN 0x80
