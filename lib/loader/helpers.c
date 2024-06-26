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

void read_from(int fd, void* buffer, size_t size, size_t offset)
{
    SYSCALL(lseek(fd, offset, SEEK_SET));
    SYSCALL(read(fd, buffer, size));
}

void* alloc_read(int fd, size_t size, size_t offset)
{
    void* buffer = ALLOC(malloc(size));
    read_from(fd, buffer, size, offset);
    return buffer;
}
