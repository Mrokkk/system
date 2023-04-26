#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define BUFFER_SIZE 256

int main(int argc, char* argv[])
{
    const char* filename = NULL;
    int fd, use_mmap = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-')
        {
            filename = argv[i];
        }
        else if (!strcmp(argv[i], "--mmap") || !strcmp(argv[i], "-m"))
        {
            use_mmap = 1;
        }
    }

    if (!filename)
    {
        printf("no file name!\n");
        return EXIT_FAILURE;
    }

    fd = open(filename, O_RDONLY, 0);

    if (fd < 0)
    {
        perror(filename);
        return EXIT_FAILURE;
    }

    if (use_mmap)
    {
        struct stat s;
        int err = stat(filename, &s);

        if (err)
        {
            perror(filename);
            return EXIT_FAILURE;
        }

        void* addr = mmap(NULL, s.st_size, PROT_READ, 0, fd, 0);
        if ((int)addr == -1)
        {
            perror(filename);
            return EXIT_FAILURE;
        }

        write(STDOUT_FILENO, addr, s.st_size);
    }
    else
    {
        int size;
        char buf[BUFFER_SIZE];

        do
        {
            size = read(fd, buf, BUFFER_SIZE);
            write(STDOUT_FILENO, buf, size);
        } while (size == BUFFER_SIZE);
    }

    return EXIT_SUCCESS;
}
