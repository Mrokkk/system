#ifndef __IO_H_
#define __IO_H_

#include <kernel/compiler.h>

/* Makra pozwalaj¹ce na odczyt pamiêci pod poszczególnymi adresami */
#define readb(address) (*(unsigned char *)(address))
#define readw(address) (*(unsigned short *)(address))
#define readl(address) (*(unsigned long *)(address))

/* Makra pozwalaj¹ce na zapis pamiêci pod poszczególnymi adresami */
#define writeb(data, address) ((*(unsigned char *)(address)) = (data))
#define writew(data, address) ((*(unsigned short *)(address)) = (data))
#define writel(data, address) ((*(unsigned long *)(address)) = (data))

/*===========================================================================*
 *                                    inb                                    *
 *===========================================================================*/
extern inline unsigned char inb(unsigned short port) {
    unsigned char rv;
    asm volatile ("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

/*===========================================================================*
 *                                   outb                                    *
 *===========================================================================*/
extern inline void outb(unsigned char data, unsigned short port) {
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

/*===========================================================================*
 *                                   inw                                     *
 *===========================================================================*/
extern inline unsigned short inw(unsigned short port) {
    unsigned short rv;
    asm volatile ("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

/*===========================================================================*
 *                                   outw                                    *
 *===========================================================================*/
extern inline void outw(unsigned short data, unsigned short port) {
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

/*===========================================================================*
 *                                   inl                                     *
 *===========================================================================*/
extern inline unsigned long inl(unsigned long port) {
    unsigned long rv;
    asm volatile ("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

/*===========================================================================*
 *                                   outl                                    *
 *===========================================================================*/
extern inline void outl(unsigned long data, unsigned long port) {
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

/*===========================================================================*
 *                                 io_wait                                   *
 *===========================================================================*/
extern inline void io_wait(void) {
    asm volatile (
        "jmp 1f;"
        "1:jmp 2f;"
        "2:"
    );
}

#endif
