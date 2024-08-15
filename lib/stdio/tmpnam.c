#include <stdio.h>

char* LIBC(tmpnam)(char* s)
{
    static unsigned nr;
    static char buf[L_tmpnam];

    if (snprintf(buf, sizeof(buf), "%s/%s_%u", P_tmpdir, s, nr++) >= (int)sizeof(buf))
    {
        return NULL;
    }

    return buf;
}

LIBC_ALIAS(tmpnam);
