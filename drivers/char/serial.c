#include <arch/io.h>
#include <arch/register.h>
#include <kernel/module.h>
#include <kernel/device.h>
#include <kernel/string.h>
#include <kernel/irq.h>
#include <kernel/buffer.h>
#include <kernel/time.h>
#include <kernel/stdarg.h>
#include <kernel/stddef.h>
#include <kernel/sys.h>
#include <kernel/mutex.h>

#define COM1 0x3f8
#define COM2 0x2f8
#define COM3 0x3e8
#define COM4 0x2e8

void serial_send(char a, int port);
int serial_open();
int serial_write();
int serial_read();
void serial_irs();

BUFFER_DECLARE(serial_buffer, 32);

char serial_status[4];

static struct file_operations fops = {
    .open = &serial_open,
    .read = &serial_read,
    .write = &serial_write,
};

KERNEL_MODULE(serial);
module_init(serial_init);
module_exit(serial_deinit);

SEMAPHORE_DECLARE(serial_sem, 1);

/*===========================================================================*
 *                                 serial_open                               *
 *===========================================================================*/
int serial_open(struct inode *inode, struct file *file) {

    unsigned short PORT;
    int minor;

    (void)inode; (void)file;

    minor = MINOR(inode->dev);

    switch (minor) {
        case 0: PORT = COM1; break;
        case 1: PORT = COM2; break;
        case 2: PORT = COM3; break;
        case 3: PORT = COM4; break;
        default: PORT = COM1;
    }

    outb(0x00, PORT + 1);    /* Disable all interrupts*/
    outb(0x80, PORT + 3);    /* Enable DLAB (set baud rate divisor) */
    outb(0x01, PORT + 0);    /* Set divisor to 3 (lo byte) 38400 baud */
    outb(0x00, PORT + 1);    /*                  (hi byte) */
    outb(0x03, PORT + 3);    /* 8 bits, no parity, one stop bit */
    outb(0xc7, PORT + 2);    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(0x0b, PORT + 4);    /* IRQs enabled, RTS/DSR set */
    serial_status[minor] = 1;

    /* Enable interrupt for receiving */
    outb(0x01, PORT + 1);

    return 0;

}

/*===========================================================================*
 *                               serial_receive                              *
 *===========================================================================*/
static inline char serial_receive(int port) {

    return inb(port);

}

/*===========================================================================*
 *                             is_transmit_empty                             *
 *===========================================================================*/
static inline int is_transmit_empty() {

    return inb(COM1 + 5) & 0x20;

}

/*===========================================================================*
 *                                serial_send                                *
 *===========================================================================*/
void serial_send(char a, int port) {

    if (a == '\n')
        serial_send('\r', port);
    while (is_transmit_empty() == 0);

    outb(a, port);

}

/*===========================================================================*
 *                                serial_irs                                 *
 *===========================================================================*/
void serial_irs() {

    char c = serial_receive(COM1);

    if (c == '\r') c = '\n';

    buffer_put(&serial_buffer, c);

    serial_send(c, COM1);

}

MUTEX_DECLARE(serial_lock);

/*===========================================================================*
 *                               serial_write                                *
 *===========================================================================*/
int serial_write(struct inode *inode, struct file *file, const char *buffer,
        unsigned int size) {

    unsigned int old = size;

    (void)inode; (void)file;

    mutex_lock(&serial_lock);

    while (size--) serial_send(*buffer++, COM1);

    mutex_unlock(&serial_lock);

    return old;

}

/*===========================================================================*
 *                               serial_read                                 *
 *===========================================================================*/
int serial_read(struct inode *inode, struct file *file, char *buffer,
        unsigned int size) {

    unsigned int i;

    (void)inode; (void)buffer; (void)size; (void)file;

    for (i=0; i<size; i++) {
        while (buffer_get(&serial_buffer, &buffer[i]));
        if (buffer[i] == '\n') return i+1;
        if (buffer[i] == '\b' && i > 0) {
            buffer[i--] = 0;
            buffer[i--] = 0;
        }
    }

    return i;

}

/*===========================================================================*
 *                                 read_line                                 *
 *===========================================================================*/
static inline void read_line(char *line) {

    int size = serial_read(0, 0, line, 32);
    line[size-1] = 0;

}

/*===========================================================================*
 *                               serial_printf                               *
 *===========================================================================*/
int serial_printf(const char *fmt, ...) {

    char printf_buf[128];
    va_list args;
    int printed;

    va_start(args, fmt);
    printed = vsprintf(printf_buf, fmt, args);
    va_end(args);

    serial_write(0, 0, printf_buf, strlen(printf_buf));

    return printed;

}

static int c_running() {

    struct process *proc;

    list_for_each_entry(proc, &running, running) {
        serial_printf("pid=%d name=%s stat=%d\n", proc->pid, proc->name,
                proc->stat);
    }
    return 0;

}

/*===========================================================================*
 *                                   c_ps                                    *
 *===========================================================================*/
static int c_ps() {

    struct process *proc;

    list_for_each_entry(proc, &init_process.processes, processes) {
        serial_printf("pid=%d name=%s stat=%c\n", proc->pid, proc->name,
                process_state_char(proc->stat));
    }

    return 0;
}

/*===========================================================================*
 *                                 c_kstat                                   *
 *===========================================================================*/
static int c_kstat() {

    #define print_data(data) \
        serial_printf(#data"=%d\n", data);

    int uptime = jiffies/HZ;

    semaphore_down(&serial_sem);

    print_data(jiffies);
    print_data(uptime);
    print_data(context_switches);
    print_data(total_forks);

    semaphore_up(&serial_sem);

    return 0;

}

int kexit() {

    return 0;

}

/*===========================================================================*
 *                                  c_zombie                                 *
 *===========================================================================*/
static int c_zombie() {

    kernel_process(&kexit, 0, 0);

    return 0;

}

/*===========================================================================*
 *                                   c_bug                                   *
 *===========================================================================*/
static int c_bug() {

    return 0;

}

#define COMMAND(name) {#name, c_##name}

static struct command {
    char *name;
    int (*function)();
} commands[] = {
        COMMAND(ps),
        COMMAND(kstat),
        COMMAND(zombie),
        COMMAND(bug),
        COMMAND(running),
        {0, 0}
};

#include <arch/register.h>

/*===========================================================================*
 *                                 seriald                                   *
 *===========================================================================*/
int seriald() {

    char line[32];
    struct command *com = commands;
    int i, pid, status;

    (void)i; (void)com; (void)line;

    serial_printf("Seriald\n");
    strcpy(process_current->name, "seriald");

    while(1) {
        serial_printf("$$ ");
        read_line(line);
        if (line[0] == 0) continue;
        for (i=0; com[i].name; i++) {
            if (!strcmp(com[i].name, line)) {
                pid = kernel_process(com[i].function, 0, 0);
                if (pid > 0) waitpid(pid, &status, 0);
                break;
            }
        }
        if (!com[i].name) {
            struct kernel_symbol *sym = symbol_find(line);
            if (sym) {
                serial_printf("%s = ", line);
                switch (sym->size) {
                    case 1:
                        serial_printf("%u", *((unsigned char *)sym->address));
                        break;
                    case 2:
                        serial_printf("%u", *((unsigned short *)sym->address));
                        break;
                    case 4:
                        serial_printf("%u", *((unsigned int *)sym->address));
                        break;
                    default:
                        serial_printf("0x%x", *((unsigned int *)sym->address));
                        break;
                }
                serial_printf(" @ 0x%x\n", sym->address);

            } else
                serial_printf("No such command: %s\n", line);
        }
    }

    return 0;

}

/*===========================================================================*
 *                               serial_init                                 *
 *===========================================================================*/
int serial_init() {

    char_device_register(MAJOR_CHR_SERIAL, "ttyS", &fops);

    irq_register(0x4, &serial_irs, "com1");
    irq_register(0x3, &serial_irs, "com2");

    kernel_process(&seriald, 0, 0);

    return 0;

}

/*===========================================================================*
 *                               serial_deinit                               *
 *===========================================================================*/
int serial_deinit() {

    serial_printf("Killing seriald\n");

    return 0;

}

