#include <mntent.h>

struct mntent* LIBC(getmntent)(FILE* stream)
{
    NOT_IMPLEMENTED(NULL, "%p", stream);
}

FILE* LIBC(setmntent)(char const* filename, char const* type)
{
    NOT_IMPLEMENTED(NULL, "\"%s\", \"%s\"", filename, type);
}

int LIBC(endmntent)(FILE* stream)
{
    NOT_IMPLEMENTED(-1, "%p", stream);
}

struct mntent* LIBC(getmntent_r)(FILE* stream, struct mntent* mntbuf, char* buf, int buflen)
{
    UNUSED(stream); UNUSED(mntbuf); UNUSED(buf); UNUSED(buflen);
    NOT_IMPLEMENTED(NULL, "%p, %p, %p, %u", stream, mntbuf, buf, buflen);
}

LIBC_ALIAS(getmntent);
LIBC_ALIAS(setmntent);
LIBC_ALIAS(endmntent);
LIBC_ALIAS(getmntent_r);
