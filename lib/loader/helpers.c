#include "loader.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void die_errno(const char* string)
{
    perror(string);
    exit(EXIT_FAILURE);
}

void die(const char* string)
{
    puts(string);
    exit(EXIT_FAILURE);
}

void* alloc_read(int fd, size_t size, size_t offset)
{
    void* buffer = ALLOC(malloc(size));
    SYSCALL(pread(fd, buffer, size, offset));
    return buffer;
}
