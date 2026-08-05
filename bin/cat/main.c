#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define BUFFER_SIZE 2048

int main(int argc, char* argv[])
{
    void* addr = NULL;
    const char* filename = NULL;
    int i, fd = -1, use_mmap = 0;

    for (i = 1; i < argc; ++i)
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
        printf("cat: no file name given\n");
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

        addr = mmap(NULL, s.st_size, PROT_READ, 0, fd, 0);
        if (addr == MAP_FAILED)
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
            if (size < 0)
            {
                perror(filename);
                return EXIT_FAILURE;
            }
            write(STDOUT_FILENO, buf, size);
        } while (size == BUFFER_SIZE);
    }

    return EXIT_SUCCESS;
}
