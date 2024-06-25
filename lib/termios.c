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

speed_t LIBC(cfgetispeed)(const struct termios* termios_p)
{
    NOT_IMPLEMENTED(15, "%p", termios_p);
}

speed_t LIBC(cfgetospeed)(const struct termios* termios_p)
{
    NOT_IMPLEMENTED(15, "%p", termios_p);
}

int LIBC(cfsetispeed)(struct termios* termios_p, speed_t speed)
{
    NOT_IMPLEMENTED(-1, "%p, %u", termios_p, speed);
}

int LIBC(cfsetospeed)(struct termios* termios_p, speed_t speed)
{
    NOT_IMPLEMENTED(-1, "%p, %u", termios_p, speed);
}

LIBC_ALIAS(tcgetattr);
LIBC_ALIAS(tcsetattr);
LIBC_ALIAS(tcflow);
LIBC_ALIAS(cfgetispeed);
LIBC_ALIAS(cfgetospeed);
LIBC_ALIAS(cfsetispeed);
LIBC_ALIAS(cfsetospeed);
