#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Font path is needed\n");
        exit(EXIT_FAILURE);
    }

    int tty_fd = open("/dev/tty", O_WRONLY);
    const char* font_path = argv[1];

    if (tty_fd == -1)
    {
        perror("/dev/tty");
        exit(EXIT_FAILURE);
    }

    int font_fd = open(font_path, O_RDONLY);

    if (font_fd == -1)
    {
        perror(font_path);
        exit(EXIT_FAILURE);
    }

    struct stat s;

    if (stat(font_path, &s) == -1)
    {
        perror(font_path);
        exit(EXIT_FAILURE);
    }

    void* addr = mmap(NULL, s.st_size, PROT_READ, 0, font_fd, 0);
    if ((int)addr == -1)
    {
        perror(font_path);
        exit(EXIT_FAILURE);
    }

    console_font_op_t op = {.data = addr, .size = s.st_size};

    if (ioctl(tty_fd, KDFONTOP, &op))
    {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }

    return 0;
}
