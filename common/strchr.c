#define FUNCNAME strchr
#include <common/defs.h>

#if !IS_ARCH_DEFINED
char* NAME(strchr)(const char* string, int c)
{
    int i, len;

    len = NAME(strlen)(string) + 1;

    for (i = 0; i < len; i++)
    {
        if (string[i] == (char)c)
        {
            return (char*)&string[i];
        }
    }

    return NULL;
}

POST(strchr);
#endif
