#pragma once

#include <stdint.h>
#include <kernel/compiler.h>

// Discussion about port 0x80 and Linux: https://groups.google.com/g/linux.kernel/c/UTKlqyBFiyU

typedef volatile uint8_t io8;
typedef volatile uint16_t io16;
typedef volatile uint32_t io32;
typedef volatile uint64_t io64;

#define readb(address) (*(io8*)(address))
#define readw(address) (*(io16*)(address))
#define readl(address) (*(io32*)(address))
#define readq(address) (*(io64*)(address))

#define writeb(data, address) ((*(io8*)(address)) = (data))
#define writew(data, address) ((*(io16*)(address)) = (data))
#define writel(data, address) ((*(io32*)(address)) = (data))
#define writeq(data, address) ((*(io64*)(address)) = (data))

static inline uint8_t inb(uint16_t port)
{
    uint8_t rv;
    asm volatile("inb %1, %0" : "=a" (rv) : "dN" (port));
    return rv;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t rv;
    asm volatile("inw %1, %0" : "=a" (rv) : "dN" (port));
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

static inline void outw(uint16_t data, uint16_t port)
{
    asm volatile("outw %0, %1" :: "a" (data), "Nd" (port));
}

static inline void outl(uint32_t data, uint16_t port)
{
    asm volatile("outl %0, %1" :: "a" (data), "Nd" (port));
}

static inline void insw(int port, void* addr, uint32_t count)
{
    asm volatile("rep; insw" : "+D" (addr), "+c" (count) : "d" (port));
}

static inline void insl(int port, void* addr, uint32_t count)
{
    asm volatile("rep; insl" : "+D" (addr), "+c" (count) : "d" (port));
}

static inline void outsw(int port, const void* addr, uint32_t count)
{
    asm volatile("rep; outsw" :: "c" (count), "d" (port), "S" (addr));
}

static inline void io_dummy(void)
{
    inb(0x80);
}

static inline void io_delay(uint32_t loops)
{
    for (uint32_t i = 0; i < loops; ++i)
    {
        io_dummy();
    }
}

