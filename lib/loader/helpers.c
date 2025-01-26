#include "loader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>

#define BUFFER_SIZE 512

static pid_t pid;

static void flush(int severity, const char* buffer)
{
    if (pid == 1)
    {
        syslog(severity, "%s", buffer);
    }
    else
    {
        fputs(buffer, stderr);
        fputc('\n', stderr);
    }
}

static void print_va_args(int severity, const char* fmt, va_list args)
{
    char buffer[BUFFER_SIZE];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    flush(severity, buffer);
}

void print(int severity, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    print_va_args(severity, fmt, args);
    va_end(args);
}

void die_errno(const char* string)
{
    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "%s: %s", string, strerror(errno));
    flush(LOG_ERR, buffer);

    exit(EXIT_FAILURE);
}

void die(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    print_va_args(LOG_ERR, fmt, args);
    va_end(args);

    exit(EXIT_FAILURE);
}

void* alloc_read(int fd, size_t size, size_t offset)
{
    void* buffer = ALLOC(malloc(size));
    SYSCALL(pread(fd, buffer, size, offset));
    return buffer;
}

void print_init(void)
{
    if ((pid = getpid()) == 1)
    {
        openlog("ld.so", LOG_PID | LOG_CONS, LOG_USER);
    }
}
