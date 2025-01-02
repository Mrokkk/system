#include "libc.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>

#define LOWCASE 0x20

static void spaces_skip(const char** nptr)
{
    for (; isspace(**nptr); ++*nptr);
}

static void sign_read(const char** nptr, bool* negative)
{
    switch (**nptr)
    {
        case '-':
            *negative = true;
            ++*nptr;
            break;
        case '+':
            ++*nptr;
            break;
    }
}

static void base_determine(const char** nptr, int* base)
{
    if (**nptr == '0')
    {
        ++*nptr;

        if ((**nptr | LOWCASE) == 'x')
        {
            if (!*base || *base == 16)
            {
                *base = 16;
                ++*nptr;
            }
        }
        else if (!*base)
        {
            *base = 8;
        }
    }
    else if (!*base)
    {
        *base = 10;
    }
}

static unsigned digit_read(char c)
{
    return isdigit(c)
        ? c - '0'
        : 10 + (c | LOWCASE) - 'a';
}

#define STRTOL(type, validate_overflow, min, max, nptr, endptr, base) \
    { \
        unsigned digit; \
        type value = 0, old; \
        bool negative = false; \
        \
        VALIDATE_INPUT((base == 0 || base >= 2) && base <= 26 && nptr, 0); \
        \
        spaces_skip(&nptr); \
        sign_read(&nptr, &negative); \
        base_determine(&nptr, &base); \
        \
        for (; *nptr; ++nptr) \
        { \
            old = value; \
            \
            digit = digit_read(*nptr); \
            \
            if (digit >= (unsigned)base) \
            { \
                break; \
            } \
            \
            value *= base; \
            value += digit; \
            \
            if (validate_overflow) \
            { \
                if (UNLIKELY(old > value)) \
                { \
                    errno = ERANGE; \
                    return negative ? min : max; \
                } \
            } \
        } \
        \
        if (endptr) \
        { \
            *endptr = (char*)nptr; \
        } \
        \
        return negative ? -value : value; \
    }

int LIBC(atoi)(const char* nptr)
{
    int base = 10;
    char** endptr = NULL;
    STRTOL(int, 0, 0, 0, nptr, endptr, base);
}

long LIBC(atol)(const char* nptr)
{
    int base = 10;
    char** endptr = NULL;
    STRTOL(long, 0, 0, 0, nptr, endptr, base);
}

long LIBC(strtol)(
    const char* nptr,
    char** endptr,
    int base)
{
    STRTOL(long, 1, LONG_MIN, LONG_MAX, nptr, endptr, base);
}

long long LIBC(strtoll)(
    const char* nptr,
    char** endptr,
    int base)
{
    STRTOL(long long, 1, LONG_LONG_MIN, LONG_LONG_MAX, nptr, endptr, base);
}

unsigned long LIBC(strtoul)(
    const char* nptr,
    char** endptr,
    int base)
{
    STRTOL(unsigned long, 1, ULONG_MAX, ULONG_MAX, nptr, endptr, base);
}

double LIBC(atof)(const char* nptr)
{
    return LIBC(atoi)(nptr);
}

LIBC_ALIAS(atoi);
LIBC_ALIAS(atol);
LIBC_ALIAS(atof);
LIBC_ALIAS(strtol);
LIBC_ALIAS(strtoll);
LIBC_ALIAS(strtoul);
