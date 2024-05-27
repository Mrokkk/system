#pragma once

#include <stdarg.h>

#define O_RDONLY            0
#define O_WRONLY            1
#define O_RDWR              2
#define O_CREAT          0x40
#define O_EXCL           0x80
#define O_NOCTTY        0x100
#define O_TRUNC         0x200
#define O_APPEND        0x400
#define O_NONBLOCK      0x800
#define O_DIRECTORY   0x10000

#define STDIN_FILENO      0
#define STDOUT_FILENO     1
#define STDERR_FILENO     2

struct libc_file
{
    int fd;
};

typedef struct libc_file FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
int fprintf(FILE* file, const char *fmt, ...);
int printf(const char *fmt, ...);

void perror(const char* s);
