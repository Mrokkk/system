#include <arch/io.h>

#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/buffer.h>
#include <kernel/device.h>
#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/process.h>

#define COM1 0x3f8
#define COM2 0x2f8
#define COM3 0x3e8
#define COM4 0x2e8

KERNEL_MODULE(serial);
module_init(serial_init);
module_exit(serial_deinit);

BUFFER_DECLARE(serial_buffer, 32);
WAIT_QUEUE_HEAD_DECLARE(serial_wq);

char serial_status[4];

void serial_send(char a, uint16_t port);
int serial_open(struct file*);
int serial_write(struct file* file, const char* buffer, int size);
int serial_read(struct file* file, char* buffer, int size);
void serial_irs(void);

static struct file_operations fops = {
    .open = &serial_open,
    .read = &serial_read,
    .write = &serial_write,
};

uint16_t minor_to_port(int minor)
{
    switch (minor)
    {
        case 0: return COM1;
        case 1: return COM2;
        case 2: return COM3;
        case 3: return COM4;
        default: return 0;
    }
}

int serial_open(struct file* file)
{
    uint16_t port;
    int minor;

    minor = MINOR(file->inode->dev);

    log_debug(DEBUG_SERIAL, "minor = %u", minor);

    if (!(port = minor_to_port(minor)))
    {
        return -EINVAL;
    }

    outb(0x00, port + 1);    // Disable all interrupts
    outb(0x80, port + 3);    // Enable DLAB (set baud rate divisor)
    outb(0x01, port + 0);    // Set divisor to 3 (lo byte) 38400 baud
    outb(0x00, port + 1);    //                  (hi byte)
    outb(0x03, port + 3);    // 8 bits, no parity, one stop bit
    outb(0xc7, port + 2);    // Enable FIFO, clear them, with 14-byte threshold
    outb(0x0b, port + 4);    // IRQs enabled, RTS/DSR set
    serial_status[minor] = 1;

    // Enable interrupt for receiving
    outb(0x01, port + 1);

    return 0;
}

static inline char serial_receive(uint16_t port)
{
    return inb(port);
}

static inline int is_transmit_empty()
{
    return inb(COM1 + 5) & 0x20;
}

void serial_send(char a, uint16_t port)
{
    if (a == '\n')
    {
        serial_send('\r', port);
    }

    while (is_transmit_empty() == 0);

    outb(a, port);
}

void serial_print(const char* string)
{
    for (; *string; serial_send(*string++, COM1));
}

void serial_irs()
{
    flags_t flags;
    char c = serial_receive(COM1);

    irq_save(flags);

    if (c == '\r')
    {
        c = '\n';
    }
    else if (c < 32)
    {
        if (c == 4) // ctrl+d
        {
            buffer_put(&serial_buffer, 4);
            goto wake_process;
        }
        else if (c == 3)
        {
            buffer_put(&serial_buffer, 3);
            goto wake_process;
        }
        // TODO: handle other chars
        goto finish;
    }
    else if (c == 0x7f) // backspace
    {
        buffer_put(&serial_buffer, '\b');
        serial_send(8, COM1);
        serial_send(' ', COM1);
        serial_send(8, COM1);
        goto wake_process;
    }

    buffer_put(&serial_buffer, c);
    serial_send(c, COM1);
wake_process:
    if (!wait_queue_empty(&serial_wq))
    {
        struct process* proc = wait_queue_pop(&serial_wq);
        process_wake(proc);
    }
finish:
    irq_restore(flags);
}

int serial_write(struct file* file, const char* buffer, int size)
{
    int old = size;
    int minor = MINOR(file->inode->dev);
    while (size--)
    {
        serial_send(*buffer++, minor_to_port(minor));
    }
    return old;
}

int serial_read(struct file*, char* buffer, int size)
{
    int i;
    WAIT_QUEUE_DECLARE(temp, process_current);

    flags_t flags;
    irq_save(flags);

    for (i = 0; i < size; i++)
    {
        // FIXME: we put process to sleep after every char; this is bad
        while (buffer_get(&serial_buffer, &buffer[i]))
        {
            wait_queue_push(&temp, &serial_wq);
            process_wait(process_current, flags);
        }
        if (buffer[i] == 4)
        {
            i = 0;
            goto finish;
        }
        else if (buffer[i] == 3)
        {
            do_kill(process_current, SIGINT);
            i = 1;
            buffer[0] = 0;
            goto finish;
        }
        if (buffer[i] == '\n')
        {
            ++i;
            goto finish;
        }
        if (buffer[i] == '\b' && i > 0)
        {
            buffer[i--] = 0;
            buffer[i--] = 0;
        }
    }

finish:
    return i;
}

int serial_init()
{
    char_device_register(MAJOR_CHR_SERIAL, "ttyS", &fops, 3, NULL);
    irq_register(0x4, &serial_irs, "com1");
    irq_register(0x3, &serial_irs, "com2");
    return 0;
}

int serial_deinit()
{
    return 0;
}
