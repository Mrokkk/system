#include <unistd.h>

int isatty(int fd)
{
    return fd > 0 && fd < 3; // FIXME: write proper implementation
}
