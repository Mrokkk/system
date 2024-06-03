#include <termios.h>

int tcgetattr(int fd, struct termios* termios_p)
{
    UNUSED(fd); UNUSED(termios_p);
    NOT_IMPLEMENTED(-1);
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p)
{
    UNUSED(fd); UNUSED(optional_actions); UNUSED(termios_p);
    NOT_IMPLEMENTED(-1);
}
