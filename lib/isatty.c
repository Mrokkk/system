#include <unistd.h>

int isatty(int fd)
{
    UNUSED(fd);
    return 1; // FIXME: write proper implementation
}
