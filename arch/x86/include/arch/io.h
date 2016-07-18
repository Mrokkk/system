#ifndef __IO_H_
#define __IO_H_

#include <kernel/compiler.h>

#define readb(address) (*(unsigned char *)(address))
#define readw(address) (*(unsigned short *)(address))
#define readl(address) (*(unsigned long *)(address))

#define writeb(data, address) ((*(unsigned char *)(address)) = (data))
#define writew(data, address) ((*(unsigned short *)(address)) = (data))
#define writel(data, address) ((*(unsigned long *)(address)) = (data))

/*===========================================================================*
 *                                    inb                                    *
 *===========================================================================*/
static inline unsigned char inb(unsigned short port) {
    unsigned char rv;
    asm volatile ("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

/*===========================================================================*
 *                                   outb                                    *
 *===========================================================================*/
static inline void outb(unsigned char data, unsigned short port) {
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

/*===========================================================================*
 *                                 io_wait                                   *
 *===========================================================================*/
static inline void io_wait(void) {
    asm volatile (
        "jmp 1f;"
        "1:jmp 2f;"
        "2:"
    );
}

#endif
