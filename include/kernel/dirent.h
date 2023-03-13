#pragma once

#include <kernel/types.h>

struct dirent
{
    int ino;
    size_t off;
    size_t len;
    char type;
    char name[256];
};

int getdents(unsigned int, void*, size_t);
