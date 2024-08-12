#include <common/defs.h>

char* NAME(strtok_r)(char* str, const char* delim, char** saveptr)
{
    if (str)
    {
        *saveptr = str;
    }

    *saveptr += NAME(strspn)(*saveptr, delim);

    if (!**saveptr)
    {
        return NULL;
    }

    char* begin = *saveptr;
    *saveptr += NAME(strcspn)(*saveptr, delim);

    if (**saveptr)
    {
        **saveptr = 0;
        ++*saveptr;
    }

    return begin;
}

POST(strtok_r);
