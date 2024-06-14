#include <errno.h>
#include <string.h>
#include <unistd.h>

int LIBC(gethostname)(char* name, size_t namelen)
{
    const char* hostname = "phoenix";
    if (namelen < strlen(hostname) + 1)
    {
        return -ENAMETOOLONG;
    }
    strcpy(name, hostname);
    return 0;
}

LIBC_ALIAS(gethostname);
