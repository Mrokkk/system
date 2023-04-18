#pragma once

#include <kernel/types.h>

#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12
#define DT_WHT      14

struct dirent
{
    int ino;
    //size_t off; // TODO: add support for this
    size_t len;
    char type;
    char name[1];
};

int getdents(unsigned int, void*, size_t);
