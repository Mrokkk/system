#pragma once

#define DMA_PRD     0x5000
#define DMA_SIZE    (2 * PAGE_SIZE)
#define DMA_START   (DMA_PRD + offsetof(dma_t, buffer))
#define DMA_EOT     (1 << 31)

struct dma;
struct prd;

typedef struct dma dma_t;
typedef struct prd prd_t;

struct prd
{
    uint32_t addr;
    uint32_t count;
} PACKED;

struct dma
{
    prd_t prdt[511];
    uint32_t buffer_size;
    uint32_t padding;
    uint8_t buffer[];
} PACKED;
