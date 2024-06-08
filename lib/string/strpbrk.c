#include <string.h>

char* strpbrk(const char* s, const char* accept)
{
    size_t al = strlen(accept);
    while (*s)
    {
        for (size_t i = 0; i < al; ++i)
        {
            if (*s == accept[i])
            {
                return (char*)s;
            }
        }
        ++s;
    }
    return NULL;
}
