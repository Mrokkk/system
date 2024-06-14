#include <string.h>

char* LIBC(strchr)(const char* string, int c)
{
    int i, len;

    len = strlen(string);

    for (i = 0; i < len; i++)
    {
        if (string[i] == (char)c)
        {
            return (char*)&string[i];
        }
    }

    return 0;
}

char* LIBC(strrchr)(const char* string, int c)
{
    int i, len, last = -1;

    len = strlen(string);

    for (i = 0; i < len; i++)
    {
        if (string[i] == (char)c) last = i;
    }

    if (last != -1)
    {
        return (char*)&string[last];
    }

    return 0;
}

LIBC_ALIAS(strchr);
LIBC_ALIAS(strrchr);
