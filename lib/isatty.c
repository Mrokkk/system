#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int isatty(int fd)
{
    struct termios t;
    return ioctl(fd, TIOCGETA, &t) == 0;
}
