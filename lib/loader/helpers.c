#include "loader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>

static void flush(const char* buffer)
{
    if (getpid() == 1)
    {
        openlog("init", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_ERR, "%s", buffer);
        closelog();
    }
    else
    {
        fputs(buffer, stderr);
        fputc('\n', stderr);
    }
}

void die_errno(const char* string)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s: %s", string, strerror(errno));
    flush(buffer);
    exit(EXIT_FAILURE);
}

void die(const char* fmt, ...)
{
    va_list args;
    char buffer[512];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    flush(buffer);

    exit(EXIT_FAILURE);
}

void* alloc_read(int fd, size_t size, size_t offset)
{
    void* buffer = ALLOC(malloc(size));
    SYSCALL(pread(fd, buffer, size, offset));
    return buffer;
}
