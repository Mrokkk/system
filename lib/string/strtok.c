#include <string.h>

char* strtok_r(char* str, const char* delim, char** saveptr)
{
    if (str)
    {
        *saveptr = str;
    }

    *saveptr += strspn(*saveptr, delim);

    if (!**saveptr)
    {
        return NULL;
    }

    char* begin = *saveptr;
    *saveptr += strcspn(*saveptr, delim);

    if (**saveptr)
    {
        **saveptr = 0;
        ++*saveptr;
    }

    return begin;
}

char* strtok(char* str, const char* delim)
{
    static char* buf;
    return strtok_r(str, delim, &buf);
}
