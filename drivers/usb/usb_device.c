#define log_fmt(fmt) "usb-dev: " fmt
#include "usb_device.h"

#include <kernel/list.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/page_alloc.h>

#include "usb.h"
#include "usb_driver.h"

static LIST_DECLARE(usb_devices);
static LIST_DECLARE(usb_drivers);

static usb_device_t* usb_device_create(
    void* hc,
    usb_hc_ops_t* ops,
    int controller_id,
    int port,
    usb_speed_t speed)
{
    usb_device_t* device = zalloc(usb_device_t);

    if (unlikely(!device))
    {
        return NULL;
    }

    device->hc = hc;
    device->controller_id = controller_id;
    device->ops = ops;
    device->port = port;
    device->speed = speed;

    return device;
}

static int usb_device_address_assign(usb_device_t* device)
{
    int errno;

    int address = device->ops->address_assign(device);

    if (unlikely(errno = errno_get(address)))
    {
        return errno;
    }

    if (unlikely(errno = usb_control_transfer(
        device,
        SET_ADDRESS_SETUP(address),
        NULL)))
    {
        return errno;
    }

    mdelay(2);

    device->address = address;

    return 0;
}

static int usb_langid_read(usb_device_t* device)
{
    int errno;
    uint16_t data[2];

    if (unlikely(errno = usb_control_transfer(
        device,
        GET_DESCRIPTOR_STRING_SETUP(0, 0, 4),
        data)))
    {
        return errno;
    }

    return data[1];
}

static const char* usb_speed_string(usb_speed_t speed)
{
    switch (speed)
    {
        case USB_SPEED_LOW: return "low speed";
        case USB_SPEED_FULL: return "full speed";
        case USB_SPEED_HIGH: return "high speed";
        default: return "unknown speed";
    }
}

static int usb_string_read(usb_device_t* device, int string_id, char* string)
{
    int errno;
    uint8_t data[256];
    usb_string_desc_t* str_desc = ptr(data);

    if (unlikely(errno = usb_control_transfer(
        device,
        GET_DESCRIPTOR_STRING_SETUP(string_id, device->langid, 1),
        str_desc)))
    {
        return errno;
    }

    if (unlikely(errno = usb_control_transfer(
        device,
        GET_DESCRIPTOR_STRING_SETUP(string_id, device->langid, str_desc->base.bLength),
        str_desc)))
    {
        return errno;
    }

    size_t len = (str_desc->base.bLength - 2) / 2;

    for (size_t i = 0; i < len; ++i)
    {
        *string++ = str_desc->bString[i] & 0x7f;
    }

    *string = 0;

    return 0;
}

static void usb_device_print(usb_device_t* device)
{
    log_info("[%s] device %02x:%02x; product: %04x:%04x; USB %x.%x compliant, LANGID: %#x",
        device->id,
        device->desc.bDeviceClass,
        device->desc.bDeviceSubClass,
        device->desc.idVendor,
        device->desc.idProduct,
        device->desc.bcdUSB >> 8,
        (device->desc.bcdUSB >> 4) & 0xf,
        device->langid);

    log_continue("; %s", usb_speed_string(device->speed));

    if (device->desc.iManufacturer)
    {
        char string[USB_STRING_SIZE];
        if (usb_string_read(device, device->desc.iManufacturer, string))
        {
            log_info("  manufacturer: <unknown> (%u)", device->desc.iManufacturer);
        }
        else
        {
            log_info("  manufacturer: %s (%u)", string, device->desc.iManufacturer);
        }
    }

    if (device->desc.iProduct)
    {
        char string[USB_STRING_SIZE];
        if (usb_string_read(device, device->desc.iProduct, string))
        {
            log_info("  product: <unknown> (%u)", device->desc.iProduct);
        }
        else
        {
            log_info("  product: %s (%u)", string, device->desc.iProduct);
        }
    }

    log_info("  configurations: %u, max packet size: %u",
        device->desc.bNumConfigurations,
        device->desc.bMaxPacketSize0);

    for (size_t i = 0; i < device->num_configurations; ++i)
    {
        usb_configuration_desc_t* config_desc = &device->configurations[i].desc;

        log_info("  configuration (%u): value: %#x, total len: %u, interfaces: %u",
            config_desc->base.bDescriptorType,
            config_desc->bConfigurationValue,
            config_desc->wTotalLength,
            config_desc->bNumInterfaces);

        if (config_desc->iConfiguration)
        {
            char string[USB_STRING_SIZE];
            if (!usb_string_read(device, config_desc->iConfiguration, string))
            {
                log_continue("; name: %s (%u)", string, config_desc->iConfiguration);
            }
        }

        usb_interface_t* interfaces = device->configurations[i].interfaces;

        for (size_t j = 0; j < device->configurations[i].num_interfaces; ++j)
        {
            usb_interface_desc_t* if_desc = &interfaces[j].desc;

            log_info("    interface (%u): class: %02x:%02x, protocol: %#x, endpoints: %u",
                if_desc->base.bDescriptorType,
                if_desc->bInterfaceClass,
                if_desc->bInterfaceSubClass,
                if_desc->bInterfaceProtocol,
                if_desc->bNumEndpoints);

            if (if_desc->iInterface)
            {
                char string[USB_STRING_SIZE];
                if (!usb_string_read(device, if_desc->iInterface, string))
                {
                    log_continue("; name: %s (%u)", string, if_desc->iInterface);
                }
            }

            usb_endpoint_t* endpoints = interfaces[j].endpoints;

            for (size_t k = 0; k < interfaces[j].num_endpoints; ++k)
            {
                usb_endpoint_desc_t* endpoint_desc = &endpoints[k].desc;

                log_info("      endpoint (%u): %s %#04x attr: %#04x, max_packet: %u, interval: %u",
                    endpoint_desc->base.bDescriptorType,
                    endpoint_desc->bEndpointAddress & 0x80 ? " IN" : "OUT",
                    endpoint_desc->bEndpointAddress & 0xf,
                    endpoint_desc->bmAttributes,
                    endpoint_desc->wMaxPacketSize,
                    endpoint_desc->bInterval);
            }
        }
    }
}

static usb_driver_ops_t* usb_driver_find(usb_device_t* device)
{
    usb_driver_ops_t* ops;

    list_for_each_entry(ops, &usb_drivers, list_entry)
    {
        if (!ops->probe(device))
        {
            return ops;
        }
    }

    return NULL;
}

static void usb_device_delete(usb_device_t* device)
{
    usb_driver_t* driver = device->driver;

    if (driver && driver->ops->release)
    {
        driver->ops->release(driver);
    }

    for (size_t c = 0; c < device->num_configurations; ++c)
    {
        usb_configuration_t* config = &device->configurations[c];
        for (size_t i = 0; i < config->num_interfaces; ++i)
        {
            usb_interface_t* interface = &config->interfaces[i];
            delete_array(interface->endpoints, interface->num_endpoints);
        }
        delete_array(config->interfaces, config->num_interfaces);
    }
    delete_array(device->configurations, device->num_configurations);

    delete(device);
}

int usb_device_detect(
    void* controller,
    usb_hc_ops_t* controller_ops,
    int controller_id,
    int port,
    usb_speed_t speed)
{
    int errno;
    uint8_t class = 0, subclass = 0;

    usb_device_t* device = usb_device_create(
        controller,
        controller_ops,
        controller_id,
        port,
        speed);

    if (unlikely(!device))
    {
        return -ENOMEM;
    }

    if (unlikely(errno = usb_control_transfer(
        device,
        GET_DESCRIPTOR_SETUP(USB_DESC_DEVICE, 8),
        &device->desc)))
    {
        goto error;
    }

    if (device->desc.base.bLength != 18 || device->desc.base.bDescriptorType != USB_DESC_DEVICE)
    {
        log_info("[%s%u-usb-%u-X] invalid device descriptor, len: %u, type: %#x",
            device->ops->name,
            device->controller_id,
            device->port,
            device->desc.base.bLength,
            device->desc.base.bDescriptorType);
        errno = -EINVAL;
        goto error;
    }

    class = device->desc.bDeviceClass;
    subclass = device->desc.bDeviceSubClass;

    if (unlikely(errno = usb_device_address_assign(device)))
    {
        goto error;
    }

    snprintf(device->id, sizeof(device->id), "%s%u-usb-%u-%u",
        device->ops->name,
        device->controller_id,
        device->port,
        device->address);

    if (unlikely(errno = usb_control_transfer(
        device,
        GET_DESCRIPTOR_SETUP(USB_DESC_DEVICE, sizeof(device->desc)),
        &device->desc)))
    {
        goto error;
    }

    if ((device->langid = usb_langid_read(device)) < 0)
    {
        log_info("cannot read langid; assuming 0x409");
        device->langid = 0x409;
    }

    device->configurations = alloc_array(usb_configuration_t, device->num_configurations);

    if (unlikely(!device->configurations))
    {
        errno = -ENOMEM;
        goto error;
    }

    device->num_configurations = device->desc.bNumConfigurations;

    memset(device->configurations, 0, sizeof(*device->configurations) * device->num_configurations);

    for (size_t i = 0; i < device->desc.bNumConfigurations; ++i)
    {
        uint8_t data[256] = {};
        usb_configuration_t* configuration = &device->configurations[i];

        if (unlikely(errno = usb_control_transfer(
            device,
            GET_DESCRIPTOR_CONFIGURATION_SETUP(i, sizeof(data)),
            data)))
        {
            continue;
        }

        void* it = data;
        void* end = it;
        usb_configuration_desc_t* config_desc = it;

        memcpy(&configuration->desc, config_desc, sizeof(*config_desc));

        it += config_desc->base.bLength;
        end += config_desc->wTotalLength;

        size_t next_if_id = 0, next_ep_id = 0, if_id = 0, ep_id = 0;

        configuration->interfaces = alloc_array(usb_interface_t, config_desc->bNumInterfaces);

        if (unlikely(!configuration->interfaces))
        {
            errno = -ENOMEM;
            goto error;
        }

        configuration->num_interfaces = config_desc->bNumInterfaces;

        while (it < end)
        {
            usb_desc_base_t* base = it;
            usb_interface_t* interfaces = configuration->interfaces;

            switch (base->bDescriptorType)
            {
                case USB_DESC_INTERFACE:
                    usb_interface_desc_t* if_desc = it;

                    if (if_desc->bNumEndpoints)
                    {
                        if_id = next_if_id++;

                        memcpy(&interfaces[if_id], if_desc, sizeof(*if_desc));

                        next_ep_id = ep_id = 0;

                        interfaces[if_id].endpoints = alloc_array(usb_endpoint_t, if_desc->bNumEndpoints);

                        if (unlikely(!interfaces[if_id].endpoints))
                        {
                            errno = -ENOMEM;
                            goto error;
                        }

                        interfaces[if_id].num_endpoints = if_desc->bNumEndpoints;

                        if (!class)
                        {
                            class = if_desc->bInterfaceClass;
                            subclass = if_desc->bInterfaceSubClass;
                        }
                    }

                    break;

                case USB_DESC_ENDPOINT:
                    usb_endpoint_desc_t* ep_desc = it;

                    ep_id = next_ep_id++;

                    interfaces[if_id].endpoints[ep_id].toggle = 0;

                    memcpy(
                        &interfaces[if_id].endpoints[ep_id].desc,
                        ep_desc,
                        sizeof(*ep_desc));

                    break;
            }

            it += base->bLength;
        }
    }

    usb_device_print(device);
    list_add(&device->list_entry, &usb_devices);

    device->class = (class << 8) | subclass;

    usb_driver_ops_t* ops = usb_driver_find(device);

    if (unlikely(!ops))
    {
        log_info("[%s] unsupported device class %#06x", device->id, device->class);
        return -ENODEV;
    }

    usb_driver_t* driver = zalloc(usb_driver_t);

    if (unlikely(!driver))
    {
        return -ENOMEM;
    }

    driver->ops = ops;
    driver->device = device;
    device->driver = driver;

    log_info("[%s] selecting driver: %s",
        device->id,
        ops->name);

    if (unlikely(errno = ops->initialize(driver)))
    {
        return errno;
    }

    return 0;

error:
    usb_device_delete(device);
    return errno;
}

static usb_packet_t* packets_create(size_t packet_count, usb_setup_t* setup, void* data)
{
    int data_packet_type;
    int status_packet_type;

    if (setup->bmRequestType & USB_PACKET_D2H)
    {
        data_packet_type = USB_PACKET_IN;
        status_packet_type = USB_PACKET_OUT;
    }
    else
    {
        data_packet_type = USB_PACKET_OUT;
        status_packet_type = USB_PACKET_IN;
    }

    usb_packet_t* packets = alloc_array(usb_packet_t, packet_count);

    if (unlikely(!packets))
    {
        return NULL;
    }

    page_t* pages = page_alloc(packet_count - 1, PAGE_ALLOC_UNCACHED);

    if (unlikely(!pages))
    {
        delete_array(packets, packet_count);
        return NULL;
    }

    size_t i = 0;

    packets[i].type = USB_PACKET_SETUP;
    packets[i].data_page = pages;
    packets[i].len = sizeof(*setup);

    memcpy(page_virt_ptr(pages), setup, sizeof(*setup));

    ++i;

    if (data)
    {
        packets[i].type = data_packet_type;
        packets[i].data_page = list_next_entry(&pages->list_entry, page_t, list_entry);
        packets[i].len = setup->wLength;
        ++i;
    }

    packets[i].type = status_packet_type;
    packets[i].data_page = NULL;
    packets[i].len = 0;

    return packets;
}

static void packets_free(usb_packet_t* packets, size_t packet_count)
{
    pages_free(packets->data_page);
    delete_array(packets, packet_count);
}

int usb_control_transfer(usb_device_t* device, usb_setup_t setup, void* data)
{
    size_t packet_count = 2 + (data ? 1 : 0);
    usb_packet_t* packets = packets_create(packet_count, &setup, data);

    if (unlikely(!packets))
    {
        return -ENOMEM;
    }

    int errno = device->ops->control_transfer(device, packets, packet_count);

    if ((setup.bmRequestType & USB_PACKET_D2H) && data)
    {
        memcpy(data, page_virt_ptr(packets[1].data_page), setup.wLength);
    }

    packets_free(packets, packet_count);

    return errno;
}

void usb_controller_port_disconnected(void* hc, int port)
{
    usb_device_t* device;

    list_for_each_entry_safe(device, &usb_devices, list_entry)
    {
        if (device->port == port && device->hc == hc)
        {
            log_info("[%s] disconnected", device->id);
            list_del(&device->list_entry);
            usb_device_delete(device);
        }
    }
}

int usb_device_configuration_set(usb_device_t* device, usb_configuration_t* config)
{
    return usb_control_transfer(
        device,
        SET_CONFIGURATION_SETUP(config->desc.bConfigurationValue),
        NULL);
}

int usb_driver_register(usb_driver_ops_t* ops)
{
    list_init(&ops->list_entry);
    list_add_tail(&ops->list_entry, &usb_drivers);
    return 0;
}
