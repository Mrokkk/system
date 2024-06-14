#include <string.h>

int LIBC(strcmp)(const char* s1, const char* s2)
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

int LIBC(strncmp)(const char* s1, const char* s2, size_t count)
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

LIBC_ALIAS(strcmp);
LIBC_ALIAS(strncmp);

WEAK_ALIAS(strcmp, strcoll);
