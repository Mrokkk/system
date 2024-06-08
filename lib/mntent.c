#include <mnent.h>

struct mntent* getmntent(FILE* stream)
{
    UNUSED(stream);
    NOT_IMPLEMENTED(NULL);
}

FILE* setmntent(char const* filename, char const* type)
{
    UNUSED(filename); UNUSED(type);
    NOT_IMPLEMENTED(NULL);
}

int endmntent(FILE* stream)
{
    UNUSED(stream);
    NOT_IMPLEMENTED(-1);
}

struct mntent* getmntent_r(FILE* stream, struct mntent* mntbuf, char* buf, int buflen)
{
    UNUSED(stream); UNUSED(mntbuf); UNUSED(buf); UNUSED(buflen);
    NOT_IMPLEMENTED(NULL);
}
