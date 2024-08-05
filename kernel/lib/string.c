#include <stddef.h>
#include <stdint.h>
#include <kernel/malloc.h>
#include <kernel/string.h>

static string_t* _string_create(size_t capacity)
{
    string_t* string;

    capacity = align(capacity, FAST_MALLOC_BLOCK_SIZE);

    if (unlikely(!(string = fmalloc(capacity + sizeof(string_t)))))
    {
        return NULL;
    }

    string->data = shift_as(char*, string, sizeof(string_t));
    string->len = 0;
    string->capacity = capacity;

    return string;
}

string_t* string_from_cstr(const char* s)
{
    string_t* string;
    size_t len = strlen(s);

    if (unlikely(!(string = _string_create(len))))
    {
        return NULL;
    }

    string->len = len;
    strcpy(string->data, s);

    return string;
}

string_t* string_from_empty(size_t len)
{
    string_t* string;

    if (unlikely(!(string = _string_create(len))))
    {
        return NULL;
    }

    *string->data = 0;

    return string;
}

void string_free(string_t* string)
{
    ffree(string, string->capacity + sizeof(*string));
}

string_it_t string_find(string_t* string, char c)
{
    return strchr(string->data, c);
}

void string_trunc(string_t* string, char* it)
{
    *it = 0;
    string->len = it - string->data;
}

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
size_t strlen(const char* string)
{
    char* temp;
    for (temp = (char*)string; *temp != 0; temp++);
    return temp - string;
}
#endif

#ifndef __HAVE_ARCH_STRNLEN
size_t strnlen(const char* s, size_t maxlen)
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

size_t strspn(const char* str, const char* accept)
{
    const char* s = str;
    char buf[256];

    __builtin_memset(buf, 0, 256);

    while (*accept)
    {
        buf[(unsigned char)*accept++] = 1;
    }

    while (buf[(unsigned char)*s])
    {
        ++s;
    }

    return s - str;
}

size_t strcspn(const char* str, const char* reject)
{
    const char* s = str;
    char buf[256];

    __builtin_memset(buf, 0, 256);
    buf[0] = 1;

    while (*reject)
    {
        buf[(unsigned char)*reject++] = 1;
    }

    while (!buf[(unsigned char)*s])
    {
        ++s;
    }

    return s - str;
}

char* strtok_r(char* str, const char* delim, char** saveptr)
{
    if (str)
    {
        *saveptr = str;
    }

    *saveptr += strspn(*saveptr, delim);

    if (!**saveptr)
    {
        return NULL;
    }

    char* begin = *saveptr;
    *saveptr += strcspn(*saveptr, delim);

    if (**saveptr)
    {
        **saveptr = 0;
        ++*saveptr;
    }

    return begin;
}

size_t strlcpy(char* dst, const char* src, size_t size)
{
    size_t len = strlen(src);

    if (unlikely(len >= size))
    {
        if (size)
        {
            memcpy(dst, src, size);
            dst[size - 1] = '\0';
        }
    }
    else
    {
        memcpy(dst, src, len + 1);
    }

    return len;
}
