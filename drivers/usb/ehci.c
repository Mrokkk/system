#define log_fmt(fmt) "ehci: " fmt
#include "ehci.h"

#include <arch/pci.h>
#include <kernel/vm.h>
#include <kernel/init.h>
#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

#include "usb.h"
#include "mem_pool.h"
#include "usb_device.h"

#define DEBUG_EHCI 0

static int ehci_init(usb_hc_t* hc, pci_device_t* pci_device);
static void ehci_ports_poll(usb_hc_t* hc);
static int ehci_address_assign(usb_device_t* device);
static int ehci_control_transfer(usb_device_t* device, usb_packet_t* packets, size_t packet_count);
static int ehci_bulk_transfer(usb_device_t* device, usb_endpoint_t* endpoint, void* data, size_t size);

READONLY static usb_hc_ops_t ops = {
    .name = "ehci",
    .init = &ehci_init,
    .ports_poll = &ehci_ports_poll,
    .address_assign = &ehci_address_assign,
    .control_transfer = &ehci_control_transfer,
    .bulk_transfer = &ehci_bulk_transfer,
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

#define WAIT_WHILE_SHORT(condition) \
    ({ \
        int timeout = 2000; \
        while ((condition) && --timeout) \
        { \
            udelay(1); \
        } \
        !timeout; \
    })

static int ehci_address_assign(usb_device_t* device)
{
    ehci_t* ehci = device->hc;
    return ++ehci->last_addr;
}

static int ehci_async_schedule_enable(ehci_t* ehci)
{
    ehci_asynclistaddr_write(ehci, ehci->qh->paddr);

    WAIT_WHILE_SHORT(ehci_asynclistaddr_read(ehci) != ehci->qh->paddr);

    {
        scoped_irq_lock();
        ehci_usbcmd_write(ehci, ehci_usbcmd_read(ehci) | USBCMD_ASYNC_SCHEDULE);
    }
    return WAIT_WHILE_SHORT(!(ehci_usbsts_read(ehci) | USBSTS_ASYNC_STS));
}

static int ehci_async_schedule_disable(ehci_t* ehci)
{
    {
        scoped_irq_lock();
        ehci_usbcmd_write(ehci, ehci_usbcmd_read(ehci) & ~USBCMD_ASYNC_SCHEDULE);
    }
    return WAIT_WHILE_SHORT(ehci_usbsts_read(ehci) & USBSTS_ASYNC_STS);
}

static mem_buffer_t* ehci_qtd_allocate(ehci_t* ehci, size_t count)
{
    mem_buffer_t* qtd_buf = mem_pool_allocate(ehci->qtd_pool, count);

    if (unlikely(!qtd_buf))
    {
        return NULL;
    }

    ehci_qtd_t* qtd = qtd_buf->vaddr;

    for (size_t i = 0; i < count; ++i)
    {
        qtd[i].paddr = qtd_buf->paddr + i * sizeof(*qtd);
    }

    return qtd_buf;
}

static mem_buffer_t* ehci_qh_allocate(ehci_t* ehci)
{
    mem_buffer_t* qh_buf = mem_pool_allocate(ehci->qh_pool, 1);

    if (unlikely(!qh_buf))
    {
        return NULL;
    }

    ehci_qh_t* qh = qh_buf->vaddr;

    qh->paddr = qh_buf->paddr;

    return qh_buf;
}

static int ehci_error_handle(ehci_t* ehci, usb_device_t* device, ehci_qh_t* qh, ehci_qtd_t* qtd, size_t packet_count, bool timeout)
{
    int errno;
    const char* error_name;

    if (timeout)
    {
        error_name = "timeout";
        errno = -ETIMEDOUT;
    }
    else if (qtd[packet_count - 1].status & QTD_STS_ERROR)
    {
        error_name = "qTD error";
        errno = -EIO;
    }
    else
    {
        error_name = "unknown error";
        errno = -EIO;
    }

    log_info("[port %u] %s; USBCMD: %#x, USBSTS: %#x, PORTSC: %#x",
        device->port,
        error_name,
        ehci_usbcmd_read(ehci),
        ehci_usbsts_read(ehci),
        ehci_portsc_read(ehci, device->port));

    log_info("  QH0 @ %p: char: %#x, current qTD: %p, next: %p",
        ehci->qh->paddr,
        ehci->qh->ep_char,
        ehci->qh->current_qtd,
        ehci->qh->next);

    log_info("  QH1 @ %p: char: %#x, current qTD: %p, next: %p",
        qh->paddr,
        qh->ep_char,
        qh->current_qtd,
        qh->next);

    for (size_t i = 0; i < packet_count; ++i)
    {
        log_info("    qTD%u @ %p: sts: %#x, next: %#x, buf: %#x",
            i,
            qtd[i].paddr,
            qtd[i].status,
            qtd[i].next,
            qtd[i].buffer_addr[0]);
    }

    return errno;
}

static int ehci_transfer_send_wait(usb_device_t* device, ehci_qh_t* qh, ehci_qtd_t* qtd, size_t packet_count)
{
    int errno = 0;
    ehci_t* ehci = device->hc;

    if (ehci_async_schedule_enable(ehci))
    {
        log_warning("[port %u] async schedule enable: timeout", device->port);
        errno = -ETIMEDOUT;
        goto disable;
    }

    if (WAIT_WHILE(!(ehci_usbsts_read(ehci) & (USBSTS_INT | USBSTS_ERRINT))))
    {
        errno = ehci_error_handle(ehci, device, qh, qtd, packet_count, true);
    }
    else if (unlikely(qtd[packet_count - 1].status & QTD_STS_ERROR))
    {
        errno = ehci_error_handle(ehci, device, qh, qtd, packet_count, false);
    }

disable:
    if (ehci_async_schedule_disable(ehci))
    {
        log_warning("[port %u] async schedule disable: timeout", device->port);
    }

    ehci_usbsts_write(ehci, USBSTS_INT | USBSTS_ERRINT | USBSTS_HOST_ERROR);

    return errno;
}

static int ehci_control_transfer(usb_device_t* device, usb_packet_t* packets, size_t packet_count)
{
    ehci_t* ehci = device->hc;
    size_t max_packet_size = device->desc.bMaxPacketSize0 ? device->desc.bMaxPacketSize0 : 8;

    mem_buffer_t* qtd_buf = ehci_qtd_allocate(ehci, packet_count);
    mem_buffer_t* qh_buf = ehci_qh_allocate(ehci);

    if (unlikely(!qtd_buf || !qh_buf))
    {
        if (qh_buf) mem_pool_free(qh_buf);
        if (qtd_buf) mem_pool_free(qtd_buf);
        return -ENOMEM;
    }

    ehci_qtd_t* qtds = qtd_buf->vaddr;
    ehci_qh_t* qh = qh_buf->vaddr;

    size_t i = 0;
    uint32_t toggle = 0;

    for (i = 0; i < packet_count; ++i, toggle ^= QTD_TOGGLE)
    {
        int type = 0;

        switch (packets[i].type)
        {
            case USB_PACKET_SETUP:
                type = QTD_PID_SETUP;
                break;
            case USB_PACKET_IN:
                type = QTD_PID_IN;
                break;
            case USB_PACKET_OUT:
                type = QTD_PID_OUT;
                break;
        }

        qtds[i].next = TERMINATE;
        qtds[i].alt_next = TERMINATE;
        qtds[i].buffer_addr[0] = packets[i].data_page ? page_phys(packets[i].data_page) : 0;
        qtds[i].status
            = QTD_TOTAL_LEN_SET(packets[i].len)
            | QTD_STS_ACTIVE
            | QTD_CERR_3
            | type
            | toggle;

        if (i > 0)
        {
            qtds[i - 1].next = qtds[i].paddr;
        }
    }

    qtds[i - 1].status |= QTD_IOC | QTD_TOGGLE;

    ehci->qh->current_qtd = 0;
    ehci->qh->next = qh->paddr | QH_NEXT_QH;
    ehci->qh->ep_char = QH_EP_CHAR_H;
    ehci->qh->overlay.next = TERMINATE;
    ehci->qh->overlay.alt_next = TERMINATE;

    qh->next = ehci->qh->paddr | QH_NEXT_QH;
    qh->overlay.next = qtds[0].paddr;
    qh->overlay.alt_next = TERMINATE;
    qh->ep_char
        = QH_MAX_PACKET_LEN_SET(max_packet_size)
        | QH_DEVICE_ADDRESS_SET(device->address)
        | QH_EP_CHAR_DTC
        | QH_EP_CHAR_EPS_HIGH;

    mb();

    int errno = ehci_transfer_send_wait(device, qh, qtds, packet_count);

    mem_pool_free(qh_buf);
    mem_pool_free(qtd_buf);

    return errno;
}

static int ehci_bulk_transfer(usb_device_t* device, usb_endpoint_t* endpoint, void* data, size_t size)
{
    ehci_t* ehci = device->hc;
    size_t max_packet_size = endpoint->desc.wMaxPacketSize;
    size_t packet_count = align(size, max_packet_size) / max_packet_size;
    uint8_t endpoint_address = endpoint->desc.bEndpointAddress & 0xf;
    uint32_t status_base
        = QTD_STS_ACTIVE
        | QTD_CERR_3
        | ((endpoint->desc.bEndpointAddress & USB_ENDPOINT_IN) ? QTD_PID_IN : QTD_PID_OUT);

    uintptr_t data_paddr = vm_paddr(addr(data), process_current->mm->pgd);

    mem_buffer_t* qtd_buf = ehci_qtd_allocate(ehci, packet_count);
    mem_buffer_t* qh_buf = ehci_qh_allocate(ehci);

    if (unlikely(!qtd_buf || !qh_buf))
    {
        if (qh_buf) mem_pool_free(qh_buf);
        if (qtd_buf) mem_pool_free(qtd_buf);
        return -ENOMEM;
    }

    ehci_qtd_t* qtds = qtd_buf->vaddr;
    ehci_qh_t* qh = qh_buf->vaddr;

    size_t i;

    for (i = 0; i < packet_count; ++i, endpoint->toggle ^= 1, size -= max_packet_size)
    {
        size_t packet_size = size > max_packet_size ? max_packet_size : size;

        qtds[i].next = TERMINATE;
        qtds[i].alt_next = TERMINATE;
        qtds[i].buffer_addr[0] = data_paddr + i * max_packet_size;
        qtds[i].status
            = QTD_TOTAL_LEN_SET(packet_size)
            | QTD_TOGGLE_SET(endpoint->toggle)
            | status_base;

        if (i > 0)
        {
            qtds[i - 1].next = qtds[i].paddr;
        }
    }

    qtds[i - 1].status |= QTD_IOC;

    ehci->qh->current_qtd = 0;
    ehci->qh->next = qh->paddr | QH_NEXT_QH;
    ehci->qh->ep_char = QH_EP_CHAR_H;
    ehci->qh->overlay.next = TERMINATE;
    ehci->qh->overlay.alt_next = TERMINATE;

    qh->next = ehci->qh->paddr | QH_NEXT_QH;
    qh->overlay.next = qtds[0].paddr;
    qh->overlay.alt_next = TERMINATE;
    qh->ep_char
        = QH_MAX_PACKET_LEN_SET(max_packet_size)
        | QH_DEVICE_ADDRESS_SET(device->address)
        | QH_ENDPOINT_ADDR_SET(endpoint_address)
        | QH_EP_CHAR_DTC
        | QH_EP_CHAR_EPS_HIGH;

    mb();

    int errno = ehci_transfer_send_wait(device, qh, qtds, packet_count);

    mem_pool_free(qh_buf);
    mem_pool_free(qtd_buf);

    return errno;
}

static bool ehci_hc_ownership_taken(ehci_t* ehci)
{
    uint32_t usblegsup;
    pci_config_read(ehci->pci, ehci->eecp, &usblegsup, sizeof(usblegsup));
    return (usblegsup & (USBLEGSUP_HC_OS_OWNERSHIP | USBLEGSUP_HC_BIOS_OWNERSHIP))
        == USBLEGSUP_HC_OS_OWNERSHIP;
}

static int ehci_hc_ownership_take(ehci_t* ehci)
{
    uint32_t usblegsup;

    if (!ehci->eecp)
    {
        return 0;
    }

    pci_config_read(ehci->pci, ehci->eecp, &usblegsup, sizeof(usblegsup));

    usblegsup |= USBLEGSUP_HC_OS_OWNERSHIP;

    pci_config_write(ehci->pci, ehci->eecp, &usblegsup, sizeof(usblegsup));

    if (WAIT_WHILE(!ehci_hc_ownership_taken(ehci)))
    {
        log_info("[hc] ownership take: timeout");
        return -ETIMEDOUT;
    }

    return 0;
}

static int ehci_stop(ehci_t* ehci)
{
    uint32_t usbcmd = ehci_usbcmd_read(ehci);

    ehci_usbcmd_write(ehci, usbcmd & ~USBCMD_RUN);

    if (WAIT_WHILE(!(ehci_usbsts_read(ehci) & USBSTS_HCHALTED)))
    {
        log_info("[hc] stop: timeout");
        return -ETIMEDOUT;
    }

    return 0;
}

static int ehci_reset(ehci_t* ehci)
{
    int errno;

    if (unlikely(errno = ehci_stop(ehci)))
    {
        return errno;
    }

    uint32_t usbcmd = ehci_usbcmd_read(ehci);
    ehci_usbcmd_write(ehci, usbcmd | USBCMD_HCRESET);

    if (WAIT_WHILE(ehci_usbcmd_read(ehci) & USBCMD_HCRESET))
    {
        log_info("[hc] reset: timeout");
        return -ETIMEDOUT;
    }

    return 0;
}

static int ehci_hc_initialize(ehci_t* ehci)
{
    ehci_frindex_write(ehci, 0);

    // Program the CTRLDSSEGMENT register with 4-Gigabyte segment where all
    // of the interface data structures are allocated
    ehci_ctrldssegment_write(ehci, 0);

    // Write the appropriate value to the USBINTR register to enable the
    // appropriate interrupts
    ehci_usbintr_write(ehci, 0);

    // Write the base address of the Periodic Frame List to the PERIODICLIST
    // BASE register. If there are no work items in the periodic schedule,
    // all elements of the Periodic Frame List should have their T-Bits set
    // to a one
    ehci_periodiclistbase_write(ehci, page_phys(ehci->periodic_list_page));

    ehci_asynclistaddr_write(ehci, ehci->qh->paddr);

    if (WAIT_WHILE(ehci_asynclistaddr_read(ehci) != ehci->qh->paddr))
    {
        log_info("[hc] setting ASYNCLISTADDR: timeout");
    }

    // Write the USBCMD register to set the desired interrupt threshold,
    // frame list size (if applicable) and turn the host controller ON
    // via setting the Run/Stop bit
    ehci_usbcmd_write(ehci, USBCMD_RUN | USBCMD_FRAME_SIZE_1024 | USBCMD_THRESHOLD_8MF);

    // Write a 1 to CONFIGFLAG register to route all ports to the EHCI
    // controller (see Section 4.2)
    ehci_configflag_write(ehci, 1);

    if (WAIT_WHILE(ehci_usbsts_read(ehci) & USBSTS_HCHALTED))
    {
        log_info("[hc] run: timeout");
        return -ETIMEDOUT;
    }

    return 0;
}

static int ehci_port_reset(ehci_t* ehci, int port)
{
    uint32_t portsc = ehci_portsc_read(ehci, port);

    ehci_portsc_write(ehci, port, portsc | PORTSC_RESET);

    mdelay(50);

    ehci_portsc_write(ehci, port, portsc | PORTSC_ENABLE);

    if (WAIT_WHILE(ehci_portsc_read(ehci, port) & PORTSC_RESET))
    {
        log_warning("[port %u] reset: timeout", port);
        return -ETIMEDOUT;
    }

    return 0;
}

static void ehci_device_detect(ehci_t* ehci, int port)
{
    int errno;

    if (unlikely(errno = usb_device_detect(
        ehci,
        &ops,
        ehci->hc_id,
        port,
        USB_SPEED_HIGH)))
    {
        log_info("[port %u] cannot detect: %s", port, errno_name(errno));
    }
}

static void ehci_port_check(ehci_t* ehci, int port, int init_portsc)
{
    uint32_t portsc = ehci_portsc_read(ehci, port);

    portsc |= init_portsc & PORTSC_CHANGE;

    if (!(portsc & PORTSC_CHANGE))
    {
        return;
    }

    if (!(portsc & PORTSC_PRESENT))
    {
        log_info("[port %u] disconnected", port);
        ehci_portsc_write(ehci, port, portsc);
        return;
    }

    if (unlikely(ehci_port_reset(ehci, port)))
    {
        return;
    }

    log_info("[port %u] present", port);

    if ((portsc = ehci_portsc_read(ehci, port)) & PORTSC_ENABLE)
    {
        log_continue("; hi-speed");
    }
    else
    {
        log_continue("; lo-speed - moving ownership to companion HC");
        ehci_portsc_write(ehci, port, portsc | PORTSC_OWNER);
        return;
    }

    log_continue("; PORTSC: %#x", ehci_portsc_read(ehci, port));

    ehci_device_detect(ehci, port);
}

static void ehci_port_init(ehci_t* ehci, int port)
{
    uint32_t portsc = ehci_portsc_read(ehci, port);

    if (unlikely(ehci_port_reset(ehci, port)))
    {
        return;
    }

    ehci_port_check(ehci, port, portsc);
}

static void ehci_ports_poll(usb_hc_t* hc)
{
    ehci_t* ehci = hc->data;
    for (size_t i = 0; i < ehci->ports; ++i)
    {
        ehci_port_check(ehci, i, 0);
    }
}

UNMAP_AFTER_INIT static int ehci_init(usb_hc_t* hc, pci_device_t* pci_device)
{
    int errno;

    if (pci_device->class != PCI_SERIAL_BUS ||
        pci_device->subclass != PCI_SERIAL_BUS_USB ||
        pci_device->prog_if != PCI_USB_EHCI)
    {
        log_error("invalid device: class %02x%02x, prog_if: %#x",
            pci_device->class,
            pci_device->subclass,
            pci_device->prog_if);
        return -ENODEV;
    }

    if (unlikely(!pci_device->bar[0].addr || !pci_device->bar[0].size))
    {
        return -EINVAL;
    }

    ehci_t* ehci = zalloc(ehci_t);

    if (unlikely(!ehci))
    {
        return -ENOMEM;
    }

    void* mmio = mmio_map_uc(pci_device->bar[0].addr, pci_device->bar[0].size, "ehci");

    if (unlikely(!mmio))
    {
        log_info("cannot map MMIO: %p", pci_device->bar[0].addr);
        errno = -ENOMEM;
        goto error;
    }

    ehci->hc_id    = hc->id;
    ehci->pci      = pci_device;
    ehci->cap_base = mmio;
    ehci->reg_base = mmio + readb(mmio);
    ehci->qtd_pool = mem_pool_create(sizeof(ehci_qtd_t));
    ehci->qh_pool  = mem_pool_create(sizeof(ehci_qh_t));

    if (unlikely(!ehci->qtd_pool || !ehci->qh_pool))
    {
        errno = -ENOMEM;
        goto error;
    }

    mem_buffer_t* qh_buf = ehci_qh_allocate(ehci);

    if (unlikely(!qh_buf))
    {
        errno = -ENOMEM;
        goto error;
    }

    ehci->qh = qh_buf->vaddr;

    page_t* page = page_alloc(1, PAGE_ALLOC_UNCACHED);

    if (unlikely(!page))
    {
        errno = -ENOMEM;
        goto error;
    }

    ehci->periodic_list_page = page;
    ehci->periodic_list = page_virt_ptr(page);

    for (int i = 0; i < 1024; ++i)
    {
        ehci->periodic_list[i] = TERMINATE;
    }

    uint32_t hcsparams = ehci_hcsparams_read(ehci);
    uint32_t hccparams = ehci_hccparams_read(ehci);
    uint32_t eecp = HCCPARAMS_EECP_GET(hccparams);

    ehci->eecp = eecp;

    if (unlikely(
        (errno = ehci_hc_ownership_take(ehci)) ||
        (errno = ehci_reset(ehci)) ||
        (errno = ehci_hc_initialize(ehci))))
    {
        goto error;
    }

    ehci->ports = HCSPARAMS_N_PORTS(hcsparams);

    log_debug(DEBUG_EHCI, "N_PORTS: %u, N_CC: %u, N_PCC: %u PPC: %u; P_INDICATOR: %u; 64bit: %u; debug port: %u",
        ehci->ports,
        HCSPARAMS_N_CC(hcsparams),
        HCSPARAMS_N_PCC(hcsparams),
        HCSPARAMS_PPC(hcsparams),
        HCSPARAMS_P_INDICATOR(hcsparams),
        HCCPARAMS_64BIT(hccparams),
        HCSPARAMS_DEBUG_PORT(hcsparams));

    log_debug(DEBUG_EHCI, "HCCPARAMS: %#x", hccparams);
    log_debug(DEBUG_EHCI, "Port Routing Rules: %u", HCSPARAMS_PRR(hcsparams));

    for (int i = 0; i < ehci->ports; ++i)
    {
        ehci_port_init(ehci, i);
    }

    ehci_usbsts_write(ehci, ehci_usbsts_read(ehci));

    hc->data = ehci;

    return 0;

error:
    if (mmio) mmio_unmap(mmio);
    if (ehci->qh_pool) mem_pool_delete(ehci->qh_pool);
    if (ehci->qtd_pool) mem_pool_delete(ehci->qtd_pool);
    if (ehci->periodic_list_page) pages_free(ehci->periodic_list_page);
    delete(ehci);
    return errno;
}

UNMAP_AFTER_INIT int ehci_register(void)
{
    return usb_hc_driver_register(&ops, PCI_USB_EHCI);
}

premodules_initcall(ehci_register);
