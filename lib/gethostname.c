#include <unistd.h>

int gethostname(char* name, size_t namelen)
{
    UNUSED(namelen);
    *name = 0;
    return 0;
}
