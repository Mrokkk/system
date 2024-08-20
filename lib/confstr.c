#include <unistd.h>
#include <string.h>

#include "confstr.h"

size_t LIBC(confstr)(int name, char* buf, size_t size)
{
    size_t nbytes;

    switch (name)
    {
        case _CS_PATH:
            nbytes = sizeof(DEFAULT_PATH);
            if (buf && size)
            {
                strlcpy(buf, DEFAULT_PATH, size);
            }
            errno = 0;
            break;

        default:
            nbytes = 0;
            errno = EINVAL;
            break;
    }

    return nbytes;
}

LIBC_ALIAS(confstr);
