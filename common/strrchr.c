#define FUNCNAME strrchr
#include <common/defs.h>

#if !IS_ARCH_DEFINED
char* NAME(strrchr)(const char* string, int c)
{
    int len;

    len = NAME(strlen)(string);

    for (; len >= 0; --len)
    {
        if (string[len] == (char)c)
        {
            return (char*)string + len;
        }
    }

    return NULL;
}

POST(strrchr);
#endif
