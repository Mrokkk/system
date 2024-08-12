#define FUNCNAME strlen
#include <common/defs.h>

#if !IS_ARCH_DEFINED
size_t NAME(strlen)(const char* string)
{
    char* temp;
    for (temp = (char*)string; *temp != 0; temp++);
    return temp - string;
}

POST(strlen);
#endif
