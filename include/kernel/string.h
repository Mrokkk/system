#pragma once

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

void bzero(void* s, size_t n);
void* memset(void* s, int c, size_t n);
void* memsetw(unsigned short*, unsigned short, size_t);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t count);

char* strcpy(char* restrict dst, const char* restrict src);
size_t strlcpy(char* dst, const char* src, size_t size);

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);

char* strchr(const char* s , int c);
char* strrchr(const char* s, int c);

char* strstr(const char* haystack, const char* needle);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);
char* strtok_r(char* str, const char* delim, char** saveptr);

#if __has_builtin(__builtin_bzero)
#define bzero(s, n) __builtin_bzero(s, n)
#endif

#if __has_builtin(__builtin_memset)
#define memset(dst, val, n) __builtin_memset(dst, val, n)
#endif

#if __has_builtin(__builtin_memcpy)
#define memcpy(dst, src, n) __builtin_memcpy(dst, src, n)
#endif

#if __has_builtin(__builtin_memmove)
#define memmove(dst, src, n) __builtin_memmove(dst, src, n)
#endif

#if __has_builtin(__builtin_memcmp)
#define memcmp(s1, s2, n)   __builtin_memcmp(s1, s2, n)
#endif

#if __has_builtin(__builtin_strlen)
#define strlen(s)           __builtin_strlen(s)
#endif

#if __has_builtin(__builtin_strnlen)
#define strnlen(s, n)       __builtin_strnlen(s, n)
#endif

#if __has_builtin(__builtin_strcpy)
#define strcpy(dst, src)    __builtin_strcpy(dst, src)
#endif

#if __has_builtin(__builtin_strlcpy)
#define strlcpy(dst, src, n) __builtin_strlcpy(dst, src, n)
#endif

#if __has_builtin(__builtin_strcmp)
#define strcmp(s1, s2)      __builtin_strcmp(s1, s2)
#endif

#if __has_builtin(__builtin_strncmp)
#define strncmp(s1, s2, n)  __builtin_strncmp(s1, s2, n)
#endif

#if __has_builtin(__builtin_strchr)
#define strchr(s, c)        __builtin_strchr(s, c)
#endif

#if __has_builtin(__builtin_strrchr)
#define strrchr(s, c)       __builtin_strrchr(s, c)
#endif

#define copy_struct(dest, src) \
    memcpy(dest, src, sizeof(typeof(*src)))

#define copy_array(dest, src, count) \
    memcpy(dest, src, sizeof(typeof(*src)) * (count))
