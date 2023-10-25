#include "input.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "macro.h"

static int input_fd;
static char input_buffer[1];

void input_initialize()
{
    input_fd = open("/dev/tty0", O_RDONLY | O_NONBLOCK, 0);

    if (!input_fd)
    {
        die_perror("open");
    }

    if (ioctl(input_fd, 2137))
    {
        die_perror("ioctl");
    }
}

char* input_read()
{
    int size = read(input_fd, input_buffer, 1);
    if (size == -1)
    {
        die_perror("read");
    }
    else if (size != 0)
    {
        return input_buffer;
    }
    return NULL;
}
