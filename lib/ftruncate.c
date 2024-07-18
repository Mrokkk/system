#include <unistd.h>

int LIBC(ftruncate)(int fildes, off_t length)
{
    NOT_IMPLEMENTED(-1, "%u, %u", fildes, length);
}

LIBC_ALIAS(ftruncate);
