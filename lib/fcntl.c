#include <fcntl.h>
#include <errno.h>

int fcntl(int fildes, int cmd, ...)
{
    UNUSED(fildes); UNUSED(cmd);
    NOT_IMPLEMENTED(-1);
}
