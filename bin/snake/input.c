#include "input.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "macro.h"

static int input_fd = -1;
static struct termios old_termios;

void input_initialize(void)
{
    struct termios t;
    if ((input_fd = open("/dev/tty0", O_RDONLY | O_NONBLOCK, 0)) == -1)
    {
        die_perror("/dev/tty0");
    }

    if (tcgetattr(input_fd, &t))
    {
        input_fd = -1;
        die("cannot get termios");
    }

    memcpy(&old_termios, &t, sizeof(t));

    t.c_lflag &= ~(ICANON | ECHO);

    tcsetattr(input_fd, 0, &t);
}

void input_deinitialize(void)
{
    if (input_fd != -1)
    {
        tcsetattr(input_fd, 0, &old_termios);
    }
}

char input_read(void)
{
    char buf;
    int size = read(input_fd, &buf, 1);
    if (size == -1)
    {
        die_perror("read");
    }
    else if (size != 0)
    {
        return buf;
    }
    return 0;
}
