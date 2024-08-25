#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <common/list.h>

#include "printf_buffer.h"

#define LAST_READ   1
#define LAST_WRITE  2

struct file
{
    int             fd;
    uint32_t        magic;
    int             last;
    bool            error;
    int             buftype;
    int             mode;
    printf_buffer_t buffer;
    list_head_t     files;
    char            pad[8];
};

#define FILE_CHECK(file) \
    ({ (file) && (file)->magic == FILE_MAGIC; })

#define FILE_FROM_BUFFER(buf) \
    ({ (FILE*)(ADDR(buf) - offsetof(FILE, buffer)); })

int file_allocate(int fd, int mode, FILE** file);
int file_flush(FILE* file);
int file_flush_all(void);
int file_setbuf(FILE* file, int flag, char* buf, size_t bufsiz);
int file_close(FILE* file);
