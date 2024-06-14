#include <grp.h>

struct group* LIBC(getgrent)(void)
{
    NOT_IMPLEMENTED_NO_ARGS(NULL);
}

void LIBC(setgrent)(void)
{
    NOT_IMPLEMENTED_NO_RET_NO_ARGS();
}

void LIBC(endgrent)(void)
{
    NOT_IMPLEMENTED_NO_RET_NO_ARGS();
}

LIBC_ALIAS(getgrent);
LIBC_ALIAS(setgrent);
LIBC_ALIAS(endgrent);
