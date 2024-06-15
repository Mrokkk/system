#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "../list.h"
#include "printf_buffer.h"

struct file
{
    int             fd;
    uint32_t        magic;
    bool            error;
    int             flags;
    printf_buffer_t buffer;
    list_head_t     files;
    char            pad[16];
};

#define FILE_CHECK(file) \
    ({ (file) && (file)->magic == FILE_MAGIC; })

#define FILE_FROM_BUFFER(buf) \
    ({ (FILE*)(ADDR(buf) - offsetof(FILE, buffer)); })

int file_allocate(int fd, FILE** file);
int file_flush(FILE* file);
int file_flush_all(void);
int file_setbuf(FILE* file, int flag, char* buf, size_t bufsiz);
int file_close(FILE* file);
