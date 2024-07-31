#pragma once

#include <arch/string.h>
#include <kernel/api/types.h>

struct string
{
    char* data;
    short len;
    short capacity;
};

typedef char* string_it_t;
typedef struct string string_t;

string_t* string_from_cstr(const char* s);
string_t* string_from_empty(size_t capacity);
void string_free(string_t* string);
string_it_t string_find(string_t* string, char c);
void string_trunc(string_t* string, string_it_t it);

#define string_create(arg) \
    _Generic((arg), \
        char*: string_from_cstr((const char*)arg), \
        default: string_from_empty((size_t)arg))

static inline void __string_free(string_t** string)
{
    if (*string)
    {
        string_free(*string);
    }
}

#define scoped_string_t CLEANUP(__string_free) string_t

#ifndef __HAVE_ARCH_STRLEN
size_t strlen(const char* s);
#endif

#ifndef __HAVE_ARCH_STRNLEN
size_t strnlen(const char* s, size_t count);
#endif

#ifndef __HAVE_ARCH_STRCPY
char* strcpy(char* __restrict, const char* __restrict);
#endif

#ifndef __HAVE_ARCH_STRNCPY
char* strncpy(char* __restrict, const char* __restrict, size_t count);
#endif

size_t strlcpy(char* dst, const char* src, size_t size);

size_t strspn(const char* str, const char* accept);
size_t strcspn(const char* str, const char* reject);
char* strtok_r(char* str, const char* delim, char** saveptr);

int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);

char* strchr(const char*, int);
char* strrchr(const char*, int);

void* memcpy(void* dest, const void* src, size_t n);
void* memcpyw(unsigned short*, const unsigned short*, size_t);
void* memset(void*, int, size_t);
void* memsetw(unsigned short*, unsigned short, size_t);

#define copy_struct(dest, src) \
    memcpy(dest, src, sizeof(typeof(*src)))

#define copy_array(dest, src, count) \
    memcpy(dest, src, sizeof(typeof(*src)) * (count))

static inline char* word_read(char* string, char* output)
{
    while (*string != '\n' && *string != '\0' && *string != ' ')
    {
        *output++ = *string++;
    }

    string++;
    *output = 0;

    return string;
}

static inline int memcmp(const void* cs, const void* ct, size_t count)
{
    const unsigned char* su1;
    const unsigned char* su2;
    int res = 0;

    for (su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
    {
        if ((res = *su1 - *su2) != 0)
        {
            break;
        }
    }
    return res;
}

static inline char* strstr(const char* s1, const char* s2)
{
    size_t l1, l2;

    l2 = strlen(s2);
    if (!l2)
    {
        return (char*)s1;
    }
    l1 = strlen(s1);
    while (l1 >= l2)
    {
        l1--;
        if (!memcmp(s1, s2, l2))
        {
            return (char*)s1;
        }
        s1++;
    }
    return 0;
}
