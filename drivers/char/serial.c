#include <arch/io.h>

#include <kernel/fs.h>
#include <kernel/irq.h>
#include <kernel/tty.h>
#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/process.h>

#define SERIAL_MINORS 4
#define COM1 0x3f8
#define COM2 0x2f8
#define COM3 0x3e8
#define COM4 0x2e8

KERNEL_MODULE(serial);
module_init(serial_init);
module_exit(serial_deinit);

void serial_send(char a, uint16_t port);
static int serial_open(tty_t* tty, file_t* file);
static int serial_close(tty_t* tty, file_t* file);
static int serial_write(tty_t* tty, const char* buffer, size_t size);
static void serial_putch(tty_t* tty, int c);
static void serial_irs(void);

static tty_driver_t serial_driver = {
    .name = "ttyS",
    .major = MAJOR_CHR_SERIAL,
    .minor_start = 0,
    .num = SERIAL_MINORS,
    .driver_data = NULL,
    .initialized = 0,
    .open = &serial_open,
    .close = &serial_close,
    .write = &serial_write,
    .putch = &serial_putch,
};

tty_t* serial_tty[SERIAL_MINORS];

static inline uint16_t minor_to_port(int minor)
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

#if 0
static inline int port_to_minor(uint16_t port)
{
    switch (port)
    {
        case COM1: return 0;
        case COM2: return 1;
        case COM3: return 2;
        case COM4: return 3;
        default: return 0;
    }
}
#endif

UNMAP_AFTER_INIT static int serial_init()
{
    tty_driver_register(&serial_driver);
    irq_register(0x4, &serial_irs, "com1", IRQ_DEFAULT);
    irq_register(0x3, &serial_irs, "com2", IRQ_DEFAULT);
    return 0;
}

static int serial_deinit()
{
    return 0;
}

static int serial_open(tty_t* tty, file_t* file)
{
    uint16_t port;
    int minor;

    minor = MINOR(file->dentry->inode->rdev);

    log_debug(DEBUG_SERIAL, "minor = %u", minor);

    if (!(port = minor_to_port(minor)))
    {
        return -EINVAL;
    }

    outb(0x00, port + 1);    // Disable all interrupts
    outb(0x80, port + 3);    // Enable DLAB (set baud rate divisor)
    outb(0x00, port + 0);    // Set divisor to 0 (lo byte) 115200 baud
    outb(0x00, port + 1);    //                  (hi byte)
    outb(0x03, port + 3);    // 8 bits, no parity, one stop bit
    outb(0xc7, port + 2);    // Enable FIFO, clear them, with 14-byte threshold
    outb(0x0b, port + 4);    // IRQs enabled, RTS/DSR set

    // Enable interrupt for receiving
    outb(0x01, port + 1);

    ASSERT(!serial_tty[minor]);
    serial_tty[minor] = tty;

    return 0;
}

static int serial_close(tty_t*, file_t*)
{
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

void serial_irs(void)
{
    char c;
    tty_t* tty = serial_tty[0];

    scoped_irq_lock();

    c = serial_receive(COM1);

    if (c == '\r')
    {
        tty_char_insert(tty, '\n');
    }
    else if (c < 32)
    {
        if (c == 4) // ctrl+d
        {
            tty_char_insert(tty, c);
        }
        else if (c == 3)
        {
            tty_char_insert(tty, c);
        }
        else if (c == '\n')
        {
            tty_char_insert(tty, c);
        }
        // TODO: handle other chars
    }
    else if (c == 0x7f) // backspace
    {
        tty_char_insert(tty, '\b');
    }
    else
    {
        tty_char_insert(tty, c);
    }
}

static int serial_write(tty_t* tty, const char* buffer, size_t size)
{
    size_t old = size;
    int port = minor_to_port(tty->minor);
    while (size--)
    {
        serial_send(*buffer++, port);
    }
    return old;
}

static void serial_putch(tty_t*, int c)
{
    serial_send(c, COM1);
}
