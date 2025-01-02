#include <unistd.h>

int symlink(const char* target, const char* linkpath)
{
    NOT_IMPLEMENTED(-1, "%s, %s", target, linkpath);
}
