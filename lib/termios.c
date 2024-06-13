#include <termios.h>

int tcgetattr(int fd, struct termios* termios_p)
{
    NOT_IMPLEMENTED(-1, "%d, %p", fd, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p)
{
    NOT_IMPLEMENTED(-1, "%d, %d, %p", fd, optional_actions, termios_p);
}

int tcflow(int fd, int action)
{
    NOT_IMPLEMENTED(-1, "%d, %d", fd, action);
}
