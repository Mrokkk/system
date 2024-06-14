#include <unistd.h>

char* LIBC(ttyname)(int fd)
{
    UNUSED(fd);
    return "/dev/tty0";
}

LIBC_ALIAS(ttyname);
