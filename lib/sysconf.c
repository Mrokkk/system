#include <unistd.h>

long LIBC(sysconf)(int name)
{
    switch (name)
    {
        case _SC_ARG_MAX: return ARG_MAX;
        case _SC_OPEN_MAX: return OPEN_MAX;
    }
    NOT_IMPLEMENTED(-1, "%u", name);
}

LIBC_ALIAS(sysconf);
