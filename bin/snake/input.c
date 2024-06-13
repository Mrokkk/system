#include "input.h"

#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <termios.h>

#include "macro.h"

static int input_fd;
static char input_buffer[1];

void input_initialize()
{
    struct termios t;
    if (!(input_fd = open("/dev/tty0", O_RDONLY | O_NONBLOCK, 0)))
    {
        die_perror("/dev/tty0");
    }

    if (tcgetattr(input_fd, &t))
    {
        die("cannot get termios");
    }

    t.c_lflag &= ~ICANON;

    tcsetattr(input_fd, 0, &t);
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
