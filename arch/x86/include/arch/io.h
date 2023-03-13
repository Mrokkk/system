#pragma once

#include <stdint.h>
#include <kernel/compiler.h>

#define readb(address) (*(uint8_t*)(address))
#define readw(address) (*(uint16_t*)(address))
#define readl(address) (*(uint32_t*)(address))

#define writeb(data, address) ((*(uint8_t*)(address)) = (data))
#define writew(data, address) ((*(uint16_t*)(address)) = (data))
#define writel(data, address) ((*(uint32_t*)(address)) = (data))

static inline uint8_t inb(uint16_t port)
{
    uint8_t rv;
    asm volatile("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

static inline void outb(uint8_t data, uint16_t port)
{
    asm volatile("outb %1, %0" : : "dN" (port), "a" (data));
}

static inline void io_wait(void)
{
    asm volatile(
        "jmp 1f;"
        "1:jmp 2f;"
        "2:"
    );
}
