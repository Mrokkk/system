#include <string.h>

char* LIBC(strstr)(const char* haystack, const char* needle)
{
    if (!*needle)
    {
        return (char*)haystack;
    }

    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);

    if (nlen > hlen)
    {
        return NULL;
    }

    for (const char* it = haystack; *it && hlen >= nlen; ++it, --hlen)
    {
        if (!memcmp(it, needle, nlen))
        {
            return (char*)it;
        }
    }

    return NULL;
}

LIBC_ALIAS(strstr);
