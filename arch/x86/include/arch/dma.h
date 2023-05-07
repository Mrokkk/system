#pragma once

#include <kernel/page.h>

#define DMA_PRD     0x1000
#define DMA_START   0x2000

struct dma;
struct prd;
struct dma_request;

typedef struct dma dma_t;
typedef struct prd prd_t;
typedef struct dma_request dma_request_t;

struct prd
{
    uint32_t addr;
    uint16_t count;
    uint16_t last;
} PACKED;

struct dma
{
    prd_t prdt[511];
    uint32_t buffer_size;
    uint32_t padding;
    uint8_t buffer[];
};
