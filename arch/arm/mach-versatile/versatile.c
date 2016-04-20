#include <arch/io.h>
#include <kernel/kernel.h>

#define UART0_BASE  0x101f1000
#define UART0_DR    0x101f1000
#define UART0_SR    0x101f1004
#define UART0_FR    0x101f1004
#define UART0_CTL             (*((volatile unsigned long *)0x101f1030))
#define UART0_IBRD            (*((volatile unsigned long *)0x101f1024))
#define UART0_FBRD            (*((volatile unsigned long *)0x101f1028))
#define UART0_LCRH            (*((volatile unsigned long *)0x101f102C))

#define UART_FR_TXFF            0x00000020
#define UART_FR_RXFE            0x00000010
#define UART_LCRH_WLEN_8        0x00000060
#define UART_LCRH_FEN           0x00000010
#define UART_CTL_UARTEN         0x00000001

void serial_send(char c) {

    while((readl(UART0_FR) & UART_FR_TXFF) != 0);

    if (c == '\n') serial_send('\r');
    writel(c, UART0_DR);

}

int serial_receive(char *c) {

    while((readl(UART0_FR) & UART_FR_RXFE) != 0);

    *c = readl(UART0_DR) & 0xff;
    if (*c == '\r') *c = '\n';
    if (*c == 0) return 1;
    printk("%c", *c);

    return 0;
}

int serial_read(int fd, char *buffer, unsigned int size) {

    unsigned int i;

    (void)buffer; (void)size; (void)fd;

    for (i=0; i<size; i++) {
        while (serial_receive(&buffer[i]));
        if (buffer[i] == '\n') return i+1;
        if (buffer[i] == '\b' && i > 0) {
            buffer[i--] = 0;
            buffer[i--] = 0;
        }
    }

    return i;

}

void serial_print(const char *s) {

    while(*s != '\0')
        serial_send(*s++);

}

int write(int fd, const char *buffer, unsigned int size) {

    (void)size;

    if (fd == 1) serial_print(buffer);

    return size;

}

int read(int fd, char *buffer, unsigned int size) {

    (void)fd; (void)buffer; (void)size;

    return serial_read(fd, buffer, size);

    return 0;

}

int arch_setup(void) {

    /* Disable UART */
    UART0_CTL &= ~UART_CTL_UARTEN;

    UART0_IBRD = 27;
    UART0_FBRD = 8;

    /* Enable UART */
    UART0_LCRH = (UART_LCRH_WLEN_8 | UART_LCRH_FEN);

    console_register(&serial_print);

    return 0;

}
