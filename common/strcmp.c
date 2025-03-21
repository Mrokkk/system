#define FUNCNAME strcmp
#include <common/defs.h>

#if !IS_ARCH_DEFINED
int NAME(strcmp)(const char* s1, const char* s2)
{
    int res;
    int val1, val2;

    while (1)
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

POST(strcmp);
#endif
