#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DIR_ALLOC_SIZE  2048

struct directory
{
    int fd;
    struct dirent* buf;
    struct dirent* current;
    size_t count;
    size_t index;
    uint32_t magic;
};

DIR* opendir(const char* name)
{
    int fd;
    DIR* dir;

    VALIDATE_INPUT(name, NULL);
    fd = SAFE_SYSCALL(open(name, O_RDONLY | O_DIRECTORY), NULL);
    dir = SAFE_ALLOC(malloc(sizeof(*dir)), NULL);

    dir->fd = fd;
    dir->magic = DIR_MAGIC;
    dir->count = 0;
    dir->index = 0;
    dir->current = NULL;
    dir->buf = NULL;

    return dir;
}

DIR* fdopendir(int fd)
{
    DIR* dir = SAFE_ALLOC(malloc(sizeof(*dir)), NULL);

    dir->fd = fd;
    dir->magic = DIR_MAGIC;
    dir->count = 0;
    dir->index = 0;
    dir->current = NULL;
    dir->buf = NULL;

    return dir;
}

int closedir(DIR* dirp)
{
    VALIDATE_INPUT(DIR_CHECK(dirp), -1);

    SAFE_SYSCALL(close(dirp->fd), -1);

    if (dirp->buf)
    {
        free(dirp->buf);
    }

    memset(dirp, 0, sizeof(*dirp));

    return 0;
}

struct dirent* readdir(DIR* dirp)
{
    VALIDATE_INPUT(DIR_CHECK(dirp), NULL);

    if (!dirp->buf)
    {
        dirp->buf = SAFE_ALLOC(malloc(DIR_ALLOC_SIZE), NULL);
    }

    if (!dirp->count)
    {
        dirp->index = 0;
        dirp->count = SAFE_SYSCALL(getdents(dirp->fd, dirp->buf, DIR_ALLOC_SIZE), NULL);
        dirp->current = dirp->buf;
    }
    else
    {
        dirp->current = (struct dirent*)((char*)dirp->current + dirp->current->d_reclen);
        dirp->index++;
    }

    if (dirp->index >= dirp->count)
    {
        dirp->index = 0;
        dirp->count = 0;
        return NULL;
    }

    return dirp->current;
}
