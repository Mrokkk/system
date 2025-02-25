#pragma once

#include <kernel/fs.h>

struct blkdev_ops
{
    int (*read)(void* blkdev, size_t offset, void* buffer, size_t size, bool irq);
    int (*medium_detect)(void* blkdev, size_t* block_size, size_t* sectors);
};

typedef struct blkdev_ops blkdev_ops_t;

struct blkdev_char
{
    const char* vendor;
    const char* model;
    uint16_t    major;
    size_t      block_size;
    size_t      sectors;
};

typedef struct blkdev_char blkdev_char_t;

int blkdev_register(blkdev_char_t* blk, void* data, blkdev_ops_t* ops);
int blkdev_free(int major, int id);
