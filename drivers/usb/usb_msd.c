#define log_fmt(fmt) "usb-msd: " fmt
#include "usb_msd.h"

#include <kernel/fs.h>
#include <kernel/dev.h>
#include <kernel/init.h>
#include <kernel/scsi.h>
#include <kernel/devfs.h>
#include <kernel/blkdev.h>
#include <kernel/kernel.h>
#include <kernel/malloc.h>
#include <kernel/byteorder.h>
#include <kernel/page_alloc.h>

#include "usb_driver.h"

static int usb_msd_probe(usb_device_t* device);
static int usb_msd_initialize(usb_driver_t* driver);
static int usb_msd_read(void* blkdev, size_t offset, void* buffer, size_t size, bool irq);

READONLY static usb_driver_ops_t ops = {
    .name = "USB Mass Storage Driver",
    .probe = &usb_msd_probe,
    .initialize = &usb_msd_initialize,
};

READONLY static blkdev_ops_t bops = {
    .read = &usb_msd_read,
};

static int usb_msd_probe(usb_device_t* device)
{
    if (device->class == 0x0806 &&
        device->num_configurations &&
        device->configurations[0].num_interfaces &&
        device->configurations[0].interfaces[0].desc.bInterfaceProtocol == 0x50)
    {
        return 0;
    }

    return -ENODEV;
}

static int usb_msd_endpoints_get(usb_driver_t* driver)
{
    usb_msd_t* msd = driver->data;
    usb_device_t* device = driver->device;
    usb_interface_t* interface = &device->configurations[0].interfaces[0];

    for (size_t i = 0; i < interface->num_endpoints; ++i)
    {
        if (interface->endpoints[i].desc.bEndpointAddress & 0x80)
        {
            msd->in = &interface->endpoints[i];
        }
        else
        {
            msd->out = &interface->endpoints[i];
        }
    }

    if (unlikely(!msd->in || !msd->out))
    {
        return -EINVAL;
    }

    return 0;
}

static int usb_msd_reset(usb_driver_t* driver)
{
    int errno;
    usb_msd_t* msd = driver->data;

    if (unlikely(errno = usb_control_transfer(driver->device, RESET_SETUP(), NULL)))
    {
        return errno;
    }

    if (unlikely(errno = usb_control_transfer(
        driver->device,
        CLEAR_ENDPOINT_HALT_SETUP(msd->in->desc.bEndpointAddress & 0xf),
        NULL)))
    {
        return errno;
    }

    if (unlikely(errno = usb_control_transfer(
        driver->device,
        CLEAR_ENDPOINT_HALT_SETUP(msd->out->desc.bEndpointAddress & 0xf),
        NULL)))
    {
        return errno;
    }

    return 0;
}

static int usb_msd_max_lun_get(usb_driver_t* driver)
{
    usb_msd_t* msd = driver->data;
    return usb_control_transfer(driver->device, GET_MAX_LUN_SETUP(), &msd->max_lun);
}

static int usb_msd_scsi_command(usb_driver_t* driver, scsi_packet_t packet, void* data, size_t size)
{
    int errno;
    usb_device_t* device = driver->device;
    usb_msd_t* msd = driver->data;
    usb_cbw_t cbw = {};
    usb_csw_t csw = {};

    cbw.signature = CBW_SIGNATURE;
    cbw.tag       = msd->last_tag++;
    cbw.len       = size;
    cbw.direction = 0x80;
    cbw.unit      = 0;
    cbw.cmd_len   = sizeof(packet);

    memcpy(cbw.cmd_data, &packet, sizeof(packet));

    if (unlikely(errno = device->ops->bulk_transfer(device, msd->out, &cbw, sizeof(cbw))))
    {
        return errno;
    }

    if (unlikely(errno = device->ops->bulk_transfer(device, msd->in, data, size)))
    {
        return errno;
    }

    if (unlikely(errno = device->ops->bulk_transfer(device, msd->in, &csw, sizeof(csw))))
    {
        return errno;
    }

    if (csw.signature == CSW_SIGNATURE && csw.status == 0 && csw.tag == cbw.tag)
    {
        return 0;
    }

    return -EIO;
}

void usb_msd_string_fix(char* s, size_t len)
{
    char* tmp = s + len - 1;

    while (*tmp == ' ' || *tmp == 0)
    {
        *tmp-- = 0;
    }

    tmp = s;
    while (*tmp == ' ')
    {
        tmp++;
    }

    if (tmp != s)
    {
        memmove(s, tmp, len - (tmp - s));
    }
}

int usb_msd_inquiry(usb_driver_t* driver)
{
    int errno;
    usb_msd_t* msd = driver->data;

    if (unlikely(errno = usb_msd_scsi_command(
        driver,
        SCSI_INQUIRY_PACKET(sizeof(msd->inquiry)),
        &msd->inquiry,
        sizeof(msd->inquiry))))
    {
        return errno;
    }

    usb_msd_string_fix(msd->inquiry.vendor_id, sizeof(msd->inquiry.vendor_id));
    usb_msd_string_fix(msd->inquiry.product_id, sizeof(msd->inquiry.product_id));

    return 0;
}

int usb_msd_capacity_read(usb_driver_t* driver)
{
    int errno;
    scsi_capacity_t capacity = {};
    usb_msd_t* msd = driver->data;

    if (unlikely(errno = usb_msd_scsi_command(driver, SCSI_READ_CAPACITY_PACKET(), &capacity, sizeof(capacity))))
    {
        return errno;
    }

    uint32_t block_size = NATIVE_U32(capacity.block_size);
    uint32_t lba = NATIVE_U32(capacity.lba) + 1;

    msd->block_size = block_size;
    msd->size = lba * block_size;

    return 0;
}

static int usb_msd_device_register(usb_driver_t* driver)
{
    usb_msd_t* msd = driver->data;

    blkdev_char_t blk = {
        .major = MAJOR_BLK_SATA,
        .vendor = msd->inquiry.vendor_id,
        .model = msd->inquiry.product_id,
        .sectors = msd->size / msd->block_size,
        .block_size = msd->block_size,
    };

    return blkdev_register(&blk, driver, &bops);
}

static int usb_msd_initialize(usb_driver_t* driver)
{
    int errno;
    page_t* msd_page = page_alloc(1, PAGE_ALLOC_ZEROED);

    if (unlikely(!msd_page))
    {
        return -ENOMEM;
    }

    usb_device_t* device = driver->device;
    usb_msd_t* msd = page_virt_ptr(msd_page);

    driver->data = msd;

    if ((errno = usb_device_configuration_set(device, &device->configurations[0])) ||
        (errno = usb_msd_endpoints_get(driver)) ||
        (errno = usb_msd_reset(driver)) ||
        (errno = usb_msd_max_lun_get(driver)) ||
        (errno = usb_msd_inquiry(driver)) ||
        (errno = usb_msd_capacity_read(driver)))
    {
        return errno;
    }

    return usb_msd_device_register(driver);
}

static int usb_msd_read(void* blkdev, size_t offset, void* buffer, size_t sectors, bool)
{
    int errno;
    usb_driver_t* driver = blkdev;
    usb_msd_t* msd = driver->data;

    if (unlikely(errno = usb_msd_scsi_command(driver, SCSI_READ_PACKET(offset, sectors), buffer, sectors * msd->block_size)))
    {
        return errno;
    }

    return 0;
}

static int usb_msd_register(void)
{
    return usb_driver_register(&ops);
}

premodules_initcall(usb_msd_register);
