#include <unistd.h>

char* ttyname(int fd)
{
    UNUSED(fd);
    return "/dev/tty0";
}
