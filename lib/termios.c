#include <termios.h>
#include <sys/ioctl.h>

int LIBC(tcgetattr)(int fd, struct termios* termios_p)
{
    return ioctl(fd, TIOCGETA, termios_p);
}

int LIBC(tcsetattr)(int fd, int optional_actions, const struct termios* termios_p)
{
    UNUSED(optional_actions);
    return ioctl(fd, TCSETA, termios_p);
}

int LIBC(tcflow)(int fd, int action)
{
    NOT_IMPLEMENTED(-1, "%d, %d", fd, action);
}

LIBC_ALIAS(tcgetattr);
LIBC_ALIAS(tcsetattr);
LIBC_ALIAS(tcflow);
