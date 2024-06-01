#include <string.h>

size_t strlen(const char* string)
{
    char* temp;
    for (temp = (char*)string; *temp != 0; temp++);
    return temp - string;
}

size_t strnlen(const char* s, size_t maxlen)
{
    size_t n;
    const char* e;
    for (e = s, n = 0; *e && n < maxlen; e++, n++);
    return n;
}
