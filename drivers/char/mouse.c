#define log_fmt(fmt) "mouse: " fmt
#include <arch/i8042.h>
#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/fifo.h>
#include <kernel/wait.h>
#include <kernel/devfs.h>
#include <kernel/ioctl.h>
#include <kernel/device.h>
#include <kernel/process.h>

static int mouse_open();
static int mouse_read(file_t*, char* data, size_t size);
static int mouse_ioctl(file_t* file, unsigned long request, void* arg);
static int mouse_poll(file_t* file, short events, short* revents, wait_queue_head_t** head);
static void mouse_irs();

#define MOUSE_RATE_10Hz     10
#define MOUSE_RATE_20Hz     20
#define MOUSE_RATE_40Hz     40
#define MOUSE_RATE_60Hz     60
#define MOUSE_RATE_80Hz     80
#define MOUSE_RATE_100Hz    100
#define MOUSE_RATE_200Hz    200

static file_operations_t fops = {
    .open = &mouse_open,
    .read = &mouse_read,
    .ioctl = &mouse_ioctl,
    .poll = &mouse_poll,
};

module_init(mouse_init);
module_exit(mouse_deinit);
KERNEL_MODULE(mouse);

BUFFER_DECLARE(mouse_fifo, 12 * 3);
WAIT_QUEUE_HEAD_DECLARE(mouse_wq);

UNMAP_AFTER_INIT int mouse_init()
{
    uint8_t byte;

    if (!i8042_is_detected(MOUSE))
    {
        log_warning("no mouse detected");
        return -ENODEV;
    }

    if (i8042_config_set(AUX_EKI))
    {
        log_warning("seting config failed");
        return -ENODEV;
    }

    i8042_send_cmd(I8042_AUX_PORT_ENABLE);

    if ((byte = i8042_send_and_receive(I8042_RATE_SET, I8042_AUX_PORT)) != I8042_RESP_ACK)
    {
        log_warning("1: setting rate failed: %x", byte);
    }

    if ((byte = i8042_send_and_receive(MOUSE_RATE_40Hz, I8042_AUX_PORT)) != I8042_RESP_ACK)
    {
        log_warning("2: setting rate failed: %x", byte);
    }

    if ((byte = i8042_send_and_receive(I8042_SCAN_ENABLE, I8042_AUX_PORT)) != I8042_RESP_ACK)
    {
        log_warning("enabling packet streaming failed: %x", byte);
    }

    return char_device_register(MAJOR_CHR_MOUSE, "mouse", &fops, 0, NULL)
        || devfs_register("mouse", MAJOR_CHR_MOUSE, 0)
        || irq_register(12, mouse_irs, "mouse", IRQ_DEFAULT);

    return 0;
}

int mouse_deinit()
{
    return 0;
}

static int mouse_open(file_t*)
{
    return 0;
}

static int mouse_read(file_t* file, char* buffer, size_t count)
{
    size_t i;
    int errno;

    WAIT_QUEUE_DECLARE(q, process_current);

    for (i = 0; i < count; i++)
    {
        while (fifo_get(&mouse_fifo, &buffer[i]))
        {
            if (file->mode & O_NONBLOCK)
            {
                return i;
            }
            if ((errno = process_wait(&mouse_wq, &q)))
            {
                buffer[0] = 0;
                return errno;
            }
        }
    }

    return count;
}

static int mouse_ioctl(file_t*, unsigned long request, void*)
{
    switch (request)
    {
        default: return -EINVAL;
    }
}

static int mouse_poll(file_t*, short events, short* revents, wait_queue_head_t** head)
{
    if (events != POLLIN)
    {
        return -EINVAL;
    }

    if (fifo_empty(&mouse_fifo))
    {
        *head = &mouse_wq;
    }
    else
    {
        *revents = POLLIN;
        *head = NULL;
    }
    return 0;
}

static void mouse_irs()
{
    uint8_t data;
    struct process* p;

    data = i8042_receive();

    fifo_put(&mouse_fifo, data);

    if (!wait_queue_empty(&mouse_wq))
    {
        p = wait_queue_front(&mouse_wq);
        process_wake(p);
    }
}
