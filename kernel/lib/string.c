#include <stddef.h>
#include <stdint.h>
#include <kernel/string.h>

#ifndef __HAVE_ARCH_STRCMP
int strcmp(const char* string1, const char* string2)
{
    if (string1 == 0 || string2 == 0) return 1;

    while (1)
    {
        if (*string1++ != *string2++)
            return 1;
        if (*string1 == '\0' && *string2 == '\0')
            return 0;
        if (*string1 == '\0' || *string2 == '\0')
            return 1;
    }

    return 0;

}
#endif

#ifndef __HAVE_ARCH_STRNCMP
int strncmp(const char* string1, const char* string2, size_t count)
{
    register char __res = 0;

    while (count)
    {
        if ((__res = *string1 - *string2++) != 0 || !*string1++)
        {
            break;
        }
        count--;
    }

    return __res;
}
#endif

#ifndef __HAVE_ARCH_STRRCHR
char* strrchr(const char* string, int c)
{
    int i, len, last = -1;

    len = strlen(string);

    for (i = 0; i < len; i++)
    {
        if (string[i] == (char)c)
            last = i;
    }

    if (last != -1)
    {
        return (char*)&string[last];
    }

    return 0;
}
#endif

#ifndef __HAVE_ARCH_STRLEN
unsigned int strlen(const char* string)
{
    char* temp;
    for (temp = (char*)string; *temp != 0; temp++);
    return temp - string;
}
#endif

#ifndef __HAVE_ARCH_STRNLEN
unsigned int strnlen(const char* s, size_t maxlen)
{
    register const char* e;
    size_t n;

    for (e = s, n = 0; *e && n < maxlen; e++, n++);

    return n;
}
#endif

#ifndef __HAVE_ARCH_STRCPY
char* strcpy(char* dest, const char* src)
{
    while ((*dest++ = *src++) != 0);
    return dest;
}
#endif

#ifndef __HAVE_ARCH_STRNCPY
char* strncpy(char* restrict dest, const char* restrict src, size_t count)
{
    size_t i;
    for (i = 0; i < count && src[i]; ++i)
    {
        dest[i] = src[i];
    }
    dest[i] = 0;
    return dest + i;
}
#endif

#ifndef __HAVE_ARCH_STRCHR
char* strchr(const char* string, int c)
{
    int i, len;

    len = strlen(string);

    for (i = 0; i < len; i++)
    {
        if (string[i] == (char)c)
        {
            return (char*)&string[i];
        }
    }

    return 0;
}
#endif

#ifndef __HAVE_ARCH_MEMCPY
void* memcpy(void* dest, const void* src, size_t size)
{
    size_t size4;
    uint32_t* d4;
    uint32_t* s4;
    uint8_t* d1;
    uint8_t* s1;

    for (size4 = size >> 2, d4 = (uint32_t*)dest, s4 = (uint32_t*)src;
         size4>0;
         size4--, *d4++ = *s4++);

    for (size = size % 4, d1 = (uint8_t*)d4, s1 = (uint8_t*)s4;
         size>0;
         size--, *d1++ = *s1++);

    return dest;
}
#endif

#ifndef __HAVE_ARCH_MEMCPYW
void* memcpyw(uint16_t* dest, const uint16_t* src, size_t count)
{
    uint16_t* s;
    uint16_t* d;

    for (s = (uint16_t*)src, d = (uint16_t*)dest;
         count != 0;
         count--, *d++ = *s++);

    return dest;
}
#endif

#ifndef __HAVE_ARCH_MEMSET
void* memset(void* ptr, int c, size_t size)
{
    for (size_t i = 0; i < size; ((char*)ptr)[i] = c, i++);
    return ptr;
}
#endif

#ifndef __HAVE_ARCH_MEMSETW
void* memsetw(uint16_t* dest, uint16_t val, size_t count)
{
    for (uint16_t* temp = (uint16_t*)dest;
         count != 0;
         count--, *temp++ = val);

    return dest;
}
#endif
