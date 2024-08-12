#define FUNCNAME strstr
#include <common/defs.h>

#if !IS_ARCH_DEFINED
char* NAME(strstr)(const char* haystack, const char* needle)
{
    if (!*needle)
    {
        return (char*)haystack;
    }

    size_t hlen = NAME(strlen)(haystack);
    size_t nlen = NAME(strlen)(needle);

    if (nlen > hlen)
    {
        return NULL;
    }

    for (const char* it = haystack; *it && hlen >= nlen; ++it, --hlen)
    {
        if (!NAME(memcmp)(it, needle, nlen))
        {
            return (char*)it;
        }
    }

    return NULL;
}

POST(strstr);
#endif
