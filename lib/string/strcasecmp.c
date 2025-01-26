#include <ctype.h>
#include <strings.h>

int LIBC(strcasecmp)(const char* s1, const char* s2)
{
    int res;
    int val1, val2;

    while (1)
    {
        val1 = *s1++;
        val2 = *s2++;
        res = tolower(val1) - tolower(val2);

        if (res || (!val1 && !val2))
        {
            break;
        }
    }

    return res;
}

int LIBC(strncasecmp)(const char* s1, const char* s2, size_t n)
{
    int res;
    int val1, val2;

    while (n--)
    {
        val1 = *s1++;
        val2 = *s2++;
        res = tolower(val1) - tolower(val2);

        if (res || (!val1 && !val2))
        {
            break;
        }
    }

    return res;
}

LIBC_ALIAS(strcasecmp);
LIBC_ALIAS(strncasecmp);
