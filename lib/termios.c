#include <termios.h>
#include <sys/ioctl.h>

int tcgetattr(int fd, struct termios* termios_p)
{
    return ioctl(fd, TIOCGETA, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p)
{
    UNUSED(optional_actions);
    return ioctl(fd, TCSETA, termios_p);
}

int tcflow(int fd, int action)
{
    NOT_IMPLEMENTED(-1, "%d, %d", fd, action);
}
