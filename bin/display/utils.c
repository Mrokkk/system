#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

void* file_map(const char* path)
{
    int fd;
    struct stat s;
    void* res;

    stat(path, &s);
    fd = open(path, O_RDONLY, 0);

    if (fd == -1)
    {
        perror(path);
        return NULL;
    }

    res = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if ((int)res == -1)
    {
        perror(path);
        return NULL;
    }

    return res;
}
