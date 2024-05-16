#pragma once

#include <stdint.h>
#include <kernel/compiler.h>

typedef volatile uint8_t io8;
typedef volatile uint16_t io16;
typedef volatile uint32_t io32;
typedef volatile uint64_t io64;

#define readb(address) (*(io8*)(address))
#define readw(address) (*(io16*)(address))
#define readl(address) (*(io32*)(address))

#define writeb(data, address) ((*(io8*)(address)) = (data))
#define writew(data, address) ((*(io16*)(address)) = (data))
#define writel(data, address) ((*(io32*)(address)) = (data))

static inline uint8_t inb(uint16_t port)
{
    uint8_t rv;
    asm volatile("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t rv;
    asm volatile("inl %1, %0" : "=a" (rv) : "Nd" (port));
    return rv;
}

static inline void outb(uint8_t data, uint16_t port)
{
    asm volatile("outb %1, %0" :: "dN" (port), "a" (data));
}

static inline void outl(uint32_t data, uint16_t port)
{
    asm volatile("outl %0, %1" :: "a" (data), "Nd" (port));
}

static inline void insw(int port, void* addr, uint32_t count)
{
    asm volatile("rep; insw" : "+D" (addr), "+c" (count) : "d" (port));
}

static inline void insl(int port, void *addr, uint32_t count)
{
    asm volatile("rep; insl" : "+D" (addr), "+c" (count) : "d" (port));
}

static inline void io_delay(uint32_t loops)
{
    for (uint32_t i = 0; i < loops; ++i)
    {
        inb(0x80);
    }
}

static inline void io_wait(void)
{
    io_delay(1);
}
