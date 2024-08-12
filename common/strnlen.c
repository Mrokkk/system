#define FUNCNAME strnlen
#include <common/defs.h>

#if !IS_ARCH_DEFINED
size_t NAME(strnlen)(const char* s, size_t maxlen)
{
    size_t n;
    const char* e;
    for (e = s, n = 0; *e && n < maxlen; e++, n++);
    return n;
}

POST(strnlen);
#endif
