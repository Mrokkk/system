#include <mnent.h>

struct mntent* getmntent(FILE* stream)
{
    NOT_IMPLEMENTED(NULL, "%p", stream);
}

FILE* setmntent(char const* filename, char const* type)
{
    NOT_IMPLEMENTED(NULL, "\"%s\", \"%s\"", filename, type);
}

int endmntent(FILE* stream)
{
    NOT_IMPLEMENTED(-1, "%p", stream);
}

struct mntent* getmntent_r(FILE* stream, struct mntent* mntbuf, char* buf, int buflen)
{
    UNUSED(stream); UNUSED(mntbuf); UNUSED(buf); UNUSED(buflen);
    NOT_IMPLEMENTED(NULL, "%p, %p, %p, %u", stream, mntbuf, buf, buflen);
}
