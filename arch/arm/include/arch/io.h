#ifndef __IO_H_
#define __IO_H_

#include <kernel/compiler.h>

#define readb(address) (*(volatile unsigned char *)(address))
#define readw(address) (*(volatile unsigned short *)(address))
#define readl(address) (*(volatile unsigned long *)(address))

#define writeb(data, address) ((*(unsigned char *)(address)) = (data))
#define writew(data, address) ((*(unsigned short *)(address)) = (data))
#define writel(data, address) ((*(unsigned long *)(address)) = (data))

/*===========================================================================*
 *                                    inb                                    *
 *===========================================================================*/
extern inline unsigned char inb(unsigned short port) {
    unsigned char rv = 0;
    (void)port;
    return rv;
}

/*===========================================================================*
 *                                   outb                                    *
 *===========================================================================*/
extern inline void outb(unsigned char data, unsigned short port) {

    (void)data; (void)port;

}

/*===========================================================================*
 *                                   inw                                     *
 *===========================================================================*/
extern inline unsigned short inw(unsigned short port) {
    unsigned short rv = 0;
    (void)port;
    return rv;
}

/*===========================================================================*
 *                                   outw                                    *
 *===========================================================================*/
extern inline void outw(unsigned short data, unsigned short port) {
    (void)data; (void)port;
}

/*===========================================================================*
 *                                   inl                                     *
 *===========================================================================*/
extern inline unsigned long inl(unsigned long port) {
    unsigned short rv = 0;
    (void)port;
    return rv;
}

/*===========================================================================*
 *                                   outl                                    *
 *===========================================================================*/
extern inline void outl(unsigned long data, unsigned long port) {
    (void)data; (void)port;
}

/*===========================================================================*
 *                                 io_wait                                   *
 *===========================================================================*/
extern inline void io_wait(void) {

}

#endif
