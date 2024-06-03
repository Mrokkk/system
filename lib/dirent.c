#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct directory
{
    int fd;
    uint32_t magic;
};

DIR* opendir(const char* name)
{
    int fd;
    DIR* dir;

    VALIDATE_INPUT(name, NULL);
    fd = SAFE_SYSCALL(open(name, O_RDONLY), NULL);
    dir = SAFE_ALLOC(malloc(sizeof(*dir)), NULL);

    dir->fd = fd;
    dir->magic = DIR_MAGIC;

    return dir;
}

DIR* fdopendir(int fd)
{
    DIR* dir = SAFE_ALLOC(malloc(sizeof(*dir)), NULL);

    dir->fd = fd;
    dir->magic = DIR_MAGIC;

    return dir;
}
