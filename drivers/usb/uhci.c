#define log_fmt(fmt) "uhci: " fmt
#include "uhci.h"

#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/vm.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/process.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

#include "usb.h"
#include "usb_hc.h"
#include "mem_pool.h"
#include "usb_device.h"

// References:
// https://ftp.netbsd.org/pub/NetBSD/misc/blymn/uhci11d.pdf

#define DEBUG_UHCI 0

static int uhci_init(usb_hc_t* hc, pci_device_t* pci_device);
static void uhci_ports_poll(usb_hc_t* hc);
static int uhci_address_assign(usb_device_t* device);
static int uhci_control_transfer(usb_device_t* device, usb_packet_t* packets, size_t count);
static int uhci_bulk_transfer(usb_device_t* device, usb_endpoint_t* endpoint, void* data, size_t size);

static int uhci_port_enable(uhci_t* uhci, int port);

READONLY static usb_hc_ops_t ops = {
    .name = "uhci",
    .init = &uhci_init,
    .ports_poll = &uhci_ports_poll,
    .address_assign = &uhci_address_assign,
    .control_transfer = &uhci_control_transfer,
    .bulk_transfer = &uhci_bulk_transfer,
};

#define WAIT_WHILE(condition) \
    ({ \
        int timeout = 5000; \
        while ((condition) && --timeout) \
        { \
            udelay(100); \
        } \
        !timeout; \
    })

#define STATUS_PRINT(status, val, name) \
    do \
    { \
        if ((status) & (val)) \
        { \
            log_continue(", " name); \
        } \
    } \
    while (0)

static int uhci_error_handle(usb_device_t* device, uhci_td_t* td, size_t packet_count, bool timeout)
{
    int errno;
    const char* error_name;
    uhci_t* uhci = device->hc;

    if (timeout)
    {
        errno = -ETIMEDOUT;
        error_name = "timeout";
    }
    else if (td[packet_count - 1].status & TD_STATUS_ERROR)
    {
        errno = -EIO;
        error_name = "TD error";
    }
    else
    {
        errno = -EIO;
        error_name = "HC error";
    }

    log_info("[port %u] %s; USBCMD: %#x, PORTSC: %#x, USBSTS: %#x",
        device->port,
        error_name,
        uhci_usbcmd_read(uhci),
        uhci_portsc_read(uhci, device->port),
        uhci_usbsts_read(uhci));

    for (size_t i = 0; i < packet_count; ++i)
    {
        log_info("  TD%u @ %p: sts: %#x, tok: %#x, buf: %#x, next: %#x",
            i,
            td[i].paddr,
            td[i].status,
            td[i].token,
            td[i].buffer_addr,
            td[i].next);
    }

    return errno;
}

static int uhci_stop(uhci_t* uhci)
{
    uhci_usbcmd_write(uhci, 0);

    if (WAIT_WHILE(!(uhci_usbsts_read(uhci) & USBSTS_HOST_CONTROLLER_HALTED)))
    {
        log_info("[hc] timeout while waiting for halt");
        return -ETIMEDOUT;
    }

    return 0;
}

static int uhci_reset(uhci_t* uhci)
{
    int errno, cmd;

    if (unlikely(errno = uhci_stop(uhci)))
    {
        return errno;
    }

    uhci_usbcmd_write(uhci, USBCMD_GLOBAL_RESET);

    mdelay(100);

    uhci_usbcmd_write(uhci, 0);

    if (WAIT_WHILE((cmd = uhci_usbcmd_read(uhci)) & USBCMD_GLOBAL_RESET))
    {
        log_notice("[hc] global reset: timeout; USBCMD: %#x", cmd);
    }

    uhci_usbcmd_write(uhci, USBCMD_HOST_CONTROLLER_RESET);

    if (WAIT_WHILE((cmd = uhci_usbcmd_read(uhci)) & USBCMD_HOST_CONTROLLER_RESET))
    {
        log_notice("[hc] reset: timeout; USBCMD: %#x", cmd);
    }

    return 0;
}

static int uhci_transfer_send_wait(usb_device_t* device, uhci_td_t* tds, size_t packet_count)
{
    int errno = 0;
    uhci_t* uhci = device->hc;
    uhci_td_t* last_td = &tds[packet_count - 1];

    uhci->skel_qh->first = tds[0].paddr;

    mb();

    if (unlikely(WAIT_WHILE(last_td->status & TD_STATUS_ACTIVE)))
    {
        errno = uhci_error_handle(device, tds, packet_count, true);
    }
    else if (unlikely(last_td->status & TD_STATUS_ERROR))
    {
        errno = uhci_error_handle(device, tds, packet_count, false);
    }

    uhci_usbsts_write(uhci, 0xff);

    WAIT_WHILE(uhci_usbsts_read(uhci));

    uhci->skel_qh->first = TERMINATE;

    return errno;
}

static int uhci_address_assign(usb_device_t* device)
{
    uhci_t* uhci = device->hc;
    return ++uhci->last_addr;
}

static mem_buffer_t* uhci_td_allocate(uhci_t* uhci, size_t count)
{
    mem_buffer_t* td_buf = mem_pool_allocate(uhci->td_pool, count);

    if (unlikely(!td_buf))
    {
        return NULL;
    }

    uhci_td_t* td = td_buf->vaddr;

    for (size_t i = 0; i < count; ++i)
    {
        td[i].paddr = td_buf->paddr + i * sizeof(*td);
    }

    return td_buf;
}

static mem_buffer_t* uhci_qh_allocate(uhci_t* uhci)
{
    mem_buffer_t* qh_buf = mem_pool_allocate(uhci->qh_pool, 1);

    if (unlikely(!qh_buf))
    {
        return NULL;
    }

    uhci_qh_t* qh = qh_buf->vaddr;

    qh->paddr = qh_buf->paddr;

    return qh_buf;
}

static int uhci_control_transfer(usb_device_t* device, usb_packet_t* packets, size_t packet_count)
{
    uhci_t* uhci = device->hc;

    scoped_mutex_lock(&uhci->lock);

    mem_buffer_t* td_buf = uhci_td_allocate(uhci, packet_count);
    size_t i = 0;

    if (unlikely(!td_buf))
    {
        return -ENOMEM;
    }

    uhci_td_t* tds = td_buf->vaddr;

    uint32_t status = (device->speed == USB_SPEED_LOW ? TD_STATUS_LOW_SPEED : 0) | TD_STATUS_ACTIVE | (3 << 27);
    uint32_t token_base = TD_TOKEN_DEVICE_ADDR_SET(device->address);

    for (i = 0; i < packet_count; ++i, token_base ^= TD_TOKEN_TOGGLE)
    {
        tds[i].buffer_addr = packets[i].data_page ? page_phys(packets[i].data_page) : 0;
        tds[i].status = status;
        tds[i].next = TERMINATE;
        tds[i].token = packets[i].type | TD_TOKEN_SIZE_SET(packets[i].len) | token_base;

        if (i > 0)
        {
            tds[i - 1].next = tds[i].paddr | TD_NEXT_DEPTH_FIRST;
        }
    }

    tds[i - 1].status |= TD_STATUS_IOC;
    tds[i - 1].token |= TD_TOKEN_TOGGLE;

    mb();

    int errno = uhci_transfer_send_wait(device, tds, packet_count);

    mem_pool_free(td_buf);

    return errno;
}

static int uhci_bulk_transfer(usb_device_t* device, usb_endpoint_t* endpoint, void* data, size_t size)
{
    uhci_t* uhci = device->hc;

    if (unlikely(!endpoint))
    {
        return -EINVAL;
    }

    scoped_mutex_lock(&uhci->lock);

    size_t max_packet_size = endpoint->desc.wMaxPacketSize;
    size_t packet_count = align(size, max_packet_size) / max_packet_size;
    uintptr_t data_paddr = vm_paddr(addr(data), process_current->mm->pgd);

    mem_buffer_t* td_buf = uhci_td_allocate(uhci, packet_count);

    if (unlikely(!td_buf))
    {
        return -ENOMEM;
    }

    uhci_td_t* tds = td_buf->vaddr;

    uint8_t endpoint_address = endpoint->desc.bEndpointAddress & 0xf;
    uint32_t status = (device->speed == USB_SPEED_LOW ? TD_STATUS_LOW_SPEED : 0) | TD_STATUS_ACTIVE;
    uint32_t token_base
        = ((endpoint->desc.bEndpointAddress & USB_ENDPOINT_IN) ? USB_PACKET_IN : USB_PACKET_OUT)
        | TD_TOKEN_DEVICE_ADDR_SET(device->address)
        | TD_TOKEN_ENDPOINT_ADDR_SET(endpoint_address);

    size_t i;

    for (i = 0; i < packet_count; ++i, endpoint->toggle ^= 1, size -= max_packet_size)
    {
        size_t packet_size = size > max_packet_size ? max_packet_size : size;

        log_debug(DEBUG_UHCI, "[%s] %s(%u): packet %u: len: %u/%u, %u",
            device->id,
            (endpoint->desc.bEndpointAddress & 0x80) ? "in" : "out",
            endpoint->desc.bEndpointAddress & 0xf,
            i,
            packet_size,
            size,
            endpoint->toggle);

        if (i > 0)
        {
            tds[i - 1].next = tds[i].paddr | TD_NEXT_DEPTH_FIRST;
        }

        tds[i].buffer_addr = data_paddr + max_packet_size * i;
        tds[i].status      = status;
        tds[i].token       = TD_TOKEN_SIZE_SET(packet_size) | TD_TOKEN_TOGGLE_SET(endpoint->toggle) | token_base;
    }

    tds[i - 1].status |= TD_STATUS_IOC;
    tds[i - 1].next = TERMINATE;

    mb();

    int errno = uhci_transfer_send_wait(device, tds, packet_count);

    mem_pool_free(td_buf);

    return errno;
}

static int uhci_port_enable(uhci_t* uhci, int port)
{
    uhci_portsc_write(uhci, port, PORTSC_RESET);

    if (WAIT_WHILE(!(uhci_portsc_read(uhci, port) & PORTSC_RESET)))
    {
        log_warning("[port %u] reset start: timeout", port);
    }

    mdelay(50);
    uhci_portsc_write(uhci, port, 0);
    mdelay(1);

    uint16_t status = uhci_portsc_read(uhci, port);

    if (!(status & PORTSC_PRESENT))
    {
        log_warning("[port %u] device not present after reset", port);
        return -ENODEV;
    }

    uhci_portsc_write(uhci, port, PORTSC_ENABLE);

    if (WAIT_WHILE(!(uhci_portsc_read(uhci, port) & PORTSC_ENABLE)))
    {
        log_warning("[port %u] enable: timeout", port);
        return -ETIMEDOUT;
    }

    uhci_portsc_write(uhci, port, PORTSC_ENABLE | PORTSC_CHANGE | PORTSC_ENABLE_CHANGE);

    return 0;
}

static void uhci_device_detect(uhci_t* uhci, int port)
{
    int errno = 0;

    log_info("[port %u] PORTSC: %#x", port, uhci_portsc_read(uhci, port));

    switch (uhci_port_enable(uhci, port))
    {
        case -ENODEV:
        case -ETIMEDOUT:
            return;
    }

    usb_speed_t speed = (uhci_portsc_read(uhci, port) & PORTSC_LOW_SPEED)
        ? USB_SPEED_LOW
        : USB_SPEED_FULL;

    log_info("[port %u] device present: %#x", port, uhci_portsc_read(uhci, port));

    if (unlikely(errno = usb_device_detect(
        uhci,
        &ops,
        uhci->hc_id,
        port,
        speed)))
    {
        log_info("[port %u] cannot detect: %s", port, errno_name(errno));
    }
}

static void uhci_port_check(uhci_t* uhci, int port)
{
    uint16_t portsc = uhci_portsc_read(uhci, port);

    if (!(portsc & PORTSC_CHANGE))
    {
        return;
    }

    if (portsc & PORTSC_PRESENT)
    {
        uhci_device_detect(uhci, port);
        return;
    }

    uhci_portsc_write(uhci, port, portsc);
    usb_controller_port_disconnected(uhci, port);
}

static void uhci_ports_poll(usb_hc_t* hc)
{
    uhci_t* uhci = hc->data;

    for (size_t i = 0; i < uhci->ports; ++i)
    {
        uhci_port_check(uhci, i);
    }
}

static int uhci_hc_init(uhci_t* uhci)
{
    int errno;

    uint16_t pci_reg = USBPIRQEN;
    pci_config_write(uhci->pci, USB_LEGKEY, &pci_reg, sizeof(pci_reg));

    if (unlikely(errno = uhci_reset(uhci)))
    {
        return errno;
    }

    for (size_t i = 0; i < UHCI_MAX_PORTS_COUNT; ++i)
    {
        if (uhci_portsc_read(uhci, i) & PORTSC_VALID)
        {
            uhci->ports++;
        }
        else
        {
            break;
        }
    }

    uhci_frbaseadd_write(uhci, page_phys(uhci->frame_list_page));
    uhci_frnum_write(uhci, 0);
    uhci_sofmod_write(uhci, 64);
    uhci_usbintr_write(uhci, 0);
    uhci_usbsts_write(uhci, 0xff);
    uhci_usbcmd_write(uhci, USBCMD_RUN | USBCMD_MAX_PACKET | USBCMD_CONFIGURE_FLAG);

    if (WAIT_WHILE(uhci_usbsts_read(uhci) & USBSTS_HOST_CONTROLLER_HALTED))
    {
        log_warning("[hc] run: timeout");
        return -ETIMEDOUT;
    }

    log_notice("ports: %u", uhci->ports);

    for (size_t i = 0; i < uhci->ports; ++i)
    {
        uhci_port_check(uhci, i);
    }

    return 0;
}

UNMAP_AFTER_INIT static int uhci_init(usb_hc_t* hc, pci_device_t* pci_device)
{
    int errno;

    if (pci_device->class != PCI_SERIAL_BUS || pci_device->subclass != PCI_SERIAL_BUS_USB || pci_device->prog_if != PCI_USB_UHCI)
    {
        log_error("invalid device: class %02x%02x, prog_if: %#x",
            pci_device->class,
            pci_device->subclass,
            pci_device->prog_if);
        return -ENODEV;
    }

    if (unlikely(!pci_device->bar[4].addr || !pci_device->bar[4].size))
    {
        return -EINVAL;
    }

    uhci_t* uhci = zalloc(uhci_t);

    if (unlikely(!uhci))
    {
        return -ENOMEM;
    }

    uhci->hc_id   = hc->id;
    uhci->iobase  = pci_device->bar[4].addr;
    uhci->pci     = pci_device;
    uhci->qh_pool = mem_pool_create(sizeof(uhci_qh_t));
    uhci->td_pool = mem_pool_create(sizeof(uhci_td_t));
    mutex_init(&uhci->lock);

    if (unlikely(!uhci->qh_pool || !uhci->td_pool))
    {
        errno = -ENOMEM;
        goto error;
    }

    mem_buffer_t* skel_qh_buf = uhci_qh_allocate(uhci);

    if (unlikely(!skel_qh_buf))
    {
        errno = -ENOMEM;
        goto error;
    }

    uhci->skel_qh = skel_qh_buf->vaddr;
    uhci->skel_qh->first = TERMINATE;
    uhci->skel_qh->next = TERMINATE;

    page_t* page = page_alloc(1, PAGE_ALLOC_ZEROED | PAGE_ALLOC_UNCACHED);

    if (unlikely(!page))
    {
        errno = -ENOMEM;
        goto error;
    }

    uhci->frame_list_page = page;
    uhci->frame_list = page_virt_ptr(page);

    for (int i = 0; i < FRLIST_SIZE; i++)
    {
        uhci->frame_list[i] = uhci->skel_qh->paddr | FRLIST_SELECT_QH;
    }

    mb();

    if (unlikely(errno = uhci_hc_init(uhci)))
    {
        goto error;
    }

    hc->data = uhci;

    return 0;

error:
    if (uhci->qh_pool) mem_pool_delete(uhci->qh_pool);
    if (uhci->td_pool) mem_pool_delete(uhci->td_pool);
    if (uhci->frame_list_page) pages_free(uhci->frame_list_page);
    delete(uhci);
    return errno;
}

UNMAP_AFTER_INIT int uhci_register(void)
{
    return usb_hc_driver_register(&ops, PCI_USB_UHCI);
}

premodules_initcall(uhci_register);
