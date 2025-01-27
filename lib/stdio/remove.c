#include <stdio.h>

int LIBC(remove)(const char* pathname)
{
    NOT_IMPLEMENTED(0, "%s", pathname);
}

LIBC_ALIAS(remove);
