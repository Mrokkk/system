#define log_fmt(fmt) "virtio-console: " fmt
#include <arch/io.h>
#include <arch/pci.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/tty.h>
#include <kernel/kernel.h>
#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/virtio.h>
#include <kernel/process.h>
#include <kernel/page_mmio.h>
#include <kernel/page_alloc.h>

KERNEL_MODULE(virtio_console);
module_init(virtio_console_init);
module_exit(virtio_console_deinit);

#define VIRTIO_CONSOLE_RECEIVEQ  0
#define VIRTIO_CONSOLE_TRANSMITQ 1
#define VIRTIO_CONSOLE_NUMQUEUES 2

typedef struct virtio_buffer virtio_buffer_t;
typedef struct virtio_console virtio_console_t;

static int virtio_console_open(tty_t* tty, file_t* file);
static int virtio_console_close(tty_t* tty, file_t* file);
static int virtio_console_write(tty_t* tty, const char* buffer, size_t size);
static void virtio_console_putch(tty_t* tty, int c);

static virtio_device_t* console;

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

static int virtio_console_write_impl(int queue_id, const void* data, size_t size)
{
    virtio_queue_t* queue = &console->queues[queue_id];
    virtio_buffer_t buffer = virtio_buffer_create(queue, size, 0);
    memcpy(buffer.buffer, data, size);
    virtio_buffer_submit(queue, &buffer);
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
    return virtio_console_write_impl(VIRTIO_CONSOLE_TRANSMITQ, buffer, size);
}

static void virtio_console_putch(tty_t*, int c)
{
    virtio_console_write_impl(VIRTIO_CONSOLE_TRANSMITQ, &c, 1);
}

UNMAP_AFTER_INIT static int virtio_console_init(void)
{
    int errno;
    pci_device_t* pci_device;

    if (unlikely(!(pci_device = pci_device_get(PCI_COMCONTROLLER, 0x80))))
    {
        log_info("no device");
        return 0;
    }

    if (unlikely(pci_device->subsystem_id != 3))
    {
        log_info("no device (wrong subsystem id: %u)", pci_device->subsystem_id);
        return 0;
    }

    if (unlikely(errno = virtio_initialize("virtio-console", pci_device, VIRTIO_CONSOLE_NUMQUEUES, &console)))
    {
        return errno;
    }

    return tty_driver_register(&tty_driver);
}

static int virtio_console_deinit()
{
    return 0;
}
