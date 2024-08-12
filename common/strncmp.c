#define FUNCNAME strncmp
#include <common/defs.h>

#if !IS_ARCH_DEFINED
int NAME(strncmp)(const char* s1, const char* s2, size_t count)
{
    int res;
    int val1, val2;

    while (count--)
    {
        val1 = *s1++;
        val2 = *s2++;
        res = val1 - val2;

        if (res || (!val1 && !val2))
        {
            break;
        }
    }

    return res;
}

POST(strncmp);
#endif
