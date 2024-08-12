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

void* memsetw(uint16_t* dest, uint16_t val, size_t count)
{
    for (uint16_t* temp = (uint16_t*)dest;
         count != 0;
         count--, *temp++ = val);

    return dest;
}
