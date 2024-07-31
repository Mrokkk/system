#define log_fmt(fmt) "virtio-console: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/tty.h>
#include <kernel/page.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/process.h>

#include "virtio.h"

KERNEL_MODULE(virtio_console);
module_init(virtio_console_init);
module_exit(virtio_console_deinit);

#define VIRTIO_CONSOLE_RECEIVEQ  0
#define VIRTIO_CONSOLE_TRANSMITQ 1
#define VIRTIO_CONSOLE_NUMQUEUES 2

#define execute(title, operation) \
    ({ int ret = operation; if (ret) { log_warning(title ": failed with %d", ret);  }; ret; })

#define execute_no_ret(operation) \
    ({ operation; 0; })

typedef struct virtio_queue virtio_queue_t;
typedef struct virtio_buffer virtio_buffer_t;
typedef struct virtio_console virtio_console_t;

struct virtio_queue
{
    page_t*        buffers;
    char*          buffer_cur;
    virtq_desc_t*  desc;
    virtq_avail_t* avail;
    virtq_used_t*  used;
    uint16_t       size;
    io32*          notify;
};

struct virtio_buffer
{
    char*     buffer;
    uintptr_t paddr;
};

struct virtio_console
{
    pci_device_t*            pci_device;
    virtio_pci_common_cfg_t* common_cfg;
    virtio_queue_t           queues[VIRTIO_CONSOLE_NUMQUEUES];
};

static int virtio_console_open(tty_t* tty, file_t* file);
static int virtio_console_close(tty_t* tty, file_t* file);
static int virtio_console_write(tty_t* tty, const char* buffer, size_t size);
static void virtio_console_putch(tty_t* tty, int c);

static virtio_console_t console;

static tty_driver_t tty_driver = {
    .name = "vcon",
    .major = MAJOR_CHR_VIRTIO_CONSOLE,
    .minor_start = 0,
    .num = 1,
    .open = &virtio_console_open,
    .close = &virtio_console_close,
    .write = &virtio_console_write,
    .putch = &virtio_console_putch,
};

static bool virtio_failed(void)
{
    return console.common_cfg->device_status & VIRTIO_FAILED;
}

static int virtio_reset(void)
{
    unsigned time_elapsed = 0;

    console.common_cfg->device_status = 0;

    while (console.common_cfg->device_status != 0)
    {
        if (unlikely(time_elapsed > 1000))
        {
            log_warning("device reset timeout");
            return -1;
        }

        udelay(10);
    }

    if (unlikely(virtio_failed()))
    {
        log_warning("device failed after reset");
        return -1;
    }

    return 0;
}

static int virtio_initialize(void)
{
    if (virtio_reset())
    {
        return -1;
    }

    console.common_cfg->device_status = VIRTIO_ACKNOWLEDGE;
    console.common_cfg->device_status |= VIRTIO_FEATURES_OK;

    if (console.common_cfg->device_status != (VIRTIO_ACKNOWLEDGE | VIRTIO_FEATURES_OK) ||
        virtio_failed())
    {
        log_warning("initialization failed; status: %x", console.common_cfg->device_status);
        return 1;
    }

    return 0;
}

static int virtio_cap_read(void** notify_bar_ptr, uint32_t* notify_cap_off, uint32_t* notify_off_multiplier)
{
    int notify_bar = 0;
    pci_cap_t cap = {};
    void* mapped_bars[6] = {};
    virtio_pci_cap_t virtio_cap[5] = {};

    for (uint8_t ptr = console.pci_device->capabilities[0] & ~0x3; ptr; ptr = cap.next)
    {
        if (pci_config_read(console.pci_device, ptr, &cap, sizeof(cap)))
        {
            log_warning("read failed");
            continue;
        }

        if (cap.id == PCI_CAP_ID_VNDR)
        {
            int index = cap.data - 1;
            pci_config_read(console.pci_device, ptr, &virtio_cap[index], sizeof(*virtio_cap));

            int bar = virtio_cap[index].bar;

            if (console.pci_device->bar[bar].region == PCI_IO)
            {
                continue;
            }

            if (!mapped_bars[bar])
            {
                mapped_bars[bar] = region_map(
                    console.pci_device->bar[bar].addr,
                    console.pci_device->bar[bar].size,
                    "virtio_console");
            }

            switch (virtio_cap[index].cfg_type)
            {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                    console.common_cfg = shift_as(
                        virtio_pci_common_cfg_t*,
                        mapped_bars[bar],
                        virtio_cap[index].offset);
                    break;
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                    pci_config_read(console.pci_device, ptr + sizeof(*virtio_cap), notify_off_multiplier, 4);
                    *notify_cap_off = virtio_cap[index].offset;
                    notify_bar = virtio_cap[index].bar;
                    break;
            }
        }
    }

    if (unlikely(!notify_bar))
    {
        return -1;
    }

    *notify_bar_ptr = mapped_bars[notify_bar];

    return 0;
}

#define VIRTQ_ALIGN        64
#define BUFFER_PAGES       4
#define BUFFER_ALLOC_FLAGS BUFFER_PAGES > 1 ? PAGE_ALLOC_CONT : PAGE_ALLOC_DISCONT

static int virtio_queues_setup(void* notify_bar, uint32_t notify_cap_off, uint32_t notify_off_multiplier)
{
    page_t* buffer;
    page_t* control;
    size_t queue_size = console.common_cfg->queue_size;

    off_t desc_off = 0;

    off_t driver_off
        = desc_off
        + sizeof(virtq_desc_t) * queue_size;

    off_t device_off
        = driver_off
        + align(sizeof(virtq_avail_t) + queue_size * sizeof(uint16_t), VIRTQ_ALIGN);

    size_t required_size
        = device_off
        + align(sizeof(virtq_used_t) + queue_size * sizeof(virtq_used_elem_t), VIRTQ_ALIGN);

    log_info("required size of control data per queue: %u B", required_size);

    if (required_size > PAGE_SIZE)
    {
        log_warning("size is bigger than page size");
        return -1;
    }

    for (int i = 0; i < VIRTIO_CONSOLE_NUMQUEUES; ++i)
    {
        buffer = page_alloc(BUFFER_PAGES, BUFFER_ALLOC_FLAGS);

        if (unlikely(!buffer))
        {
            return -ENOMEM;
        }

        control = page_alloc(1, PAGE_ALLOC_UNCACHED);

        if (unlikely(!control))
        {
            pages_free(buffer);
            return -ENOMEM;
        }

        memset(page_virt_ptr(buffer), 0, PAGE_SIZE);
        memset(page_virt_ptr(control), 0, PAGE_SIZE);

        console.common_cfg->queue_select = i;
        console.common_cfg->queue_desc = page_phys(control) + desc_off;
        console.common_cfg->queue_driver = page_phys(control) + driver_off;
        console.common_cfg->queue_device = page_phys(control) + device_off;

        console.queues[i].buffers = buffer;
        console.queues[i].buffer_cur = page_virt_ptr(buffer);
        console.queues[i].buffer_cur = page_virt_ptr(buffer);
        console.queues[i].desc = shift_as(void*, page_virt(control), desc_off);
        console.queues[i].avail = shift_as(void*, page_virt(control), driver_off);
        console.queues[i].used = shift_as(void*, page_virt(control), device_off);

        console.queues[i].avail->flags = VIRTQ_AVAIL_F_NO_INTERRUPT;
        console.queues[i].used->flags = VIRTQ_USED_F_NO_NOTIFY;

        console.queues[i].size = queue_size;

        if (unlikely(virtio_failed()))
        {
            log_warning("queue setup failed");
            return -1;
        }
    }

    for (int i = 0; i < VIRTIO_CONSOLE_NUMQUEUES; ++i)
    {
        console.common_cfg->queue_select = i;
        console.common_cfg->queue_enable = 1;

        if (unlikely(virtio_failed()))
        {
            log_warning("queue setup failed");
            return -1;
        }

        console.queues[i].notify = shift_as(
            void*,
            notify_bar,
            notify_cap_off + console.common_cfg->queue_notify_off * notify_off_multiplier);
    }

    return 0;
}

static virtio_buffer_t virtio_buffer_copy(virtio_queue_t* queue, const char* data, size_t size)
{
    char* buffer = queue->buffer_cur;

    queue->buffer_cur += size;

    if (queue->buffer_cur - (char*)page_virt_ptr(queue->buffers) >= BUFFER_PAGES * PAGE_SIZE)
    {
        buffer = page_virt_ptr(queue->buffers);
        queue->buffer_cur = buffer + size;
    }

    memcpy(buffer, data, size);

    return (virtio_buffer_t){.buffer = buffer, .paddr = phys_addr(buffer)};
}

static int virtio_write(int queue_id, const char* data, size_t size)
{
    virtio_queue_t* queue = &console.queues[queue_id];
    uint16_t id = queue->avail->idx % queue->size;
    virtio_buffer_t buffer = virtio_buffer_copy(queue, data, size);

    queue->desc[id].addr = buffer.paddr;
    queue->desc[id].flags = 0;
    queue->desc[id].len = size;
    queue->desc[id].next = 0;

    queue->avail->ring[id] = id;
    mb();

    queue->avail->idx++;
    mb();

    *queue->notify = queue_id;

    return size;
}

static int virtio_console_open(tty_t*, file_t*)
{
    return 0;
}

static int virtio_console_close(tty_t*, file_t*)
{
    return 0;
}

static int virtio_console_write(tty_t*, const char* buffer, size_t size)
{
    return virtio_write(VIRTIO_CONSOLE_TRANSMITQ, buffer, size);
}

static void virtio_console_putch(tty_t*, int c)
{
    char buffer[] = {c};
    virtio_write(VIRTIO_CONSOLE_TRANSMITQ, buffer, 1);
}

UNMAP_AFTER_INIT static int virtio_console_init()
{
    void* notify_bar = NULL;
    uint32_t notify_cap_off = 0;
    uint32_t notify_off_multiplier = 0;

    if (!(console.pci_device = pci_device_get(PCI_COMCONTROLLER, 0x80)))
    {
        log_info("no device");
        return 0;
    }

    if (console.pci_device->subsystem_id != 3)
    {
        log_info("no device (wrong subsystem id: %u)", console.pci_device->subsystem_id);
        console.pci_device = NULL;
        return 0;
    }

    if (execute_no_ret(pci_device_print(console.pci_device)) ||
        execute("initializing PCI",    pci_device_initialize(console.pci_device)) ||
        execute("reading cap",         virtio_cap_read(&notify_bar, &notify_cap_off, &notify_off_multiplier)) ||
        execute("initializing virtio", virtio_initialize()) ||
        execute("initializing queues", virtio_queues_setup(notify_bar, notify_cap_off, notify_off_multiplier)))
    {
        return -ENODEV;
    }

    console.common_cfg->device_status |= VIRTIO_DRIVER_OK;

    return tty_driver_register(&tty_driver);
}

static int virtio_console_deinit()
{
    return 0;
}
