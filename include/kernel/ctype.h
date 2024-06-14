#pragma once

#include <kernel/compiler.h>

static inline int isalnum(int c)
{
    return (c > 64 && c < 91) || (c > 96 && c < 123) || (c > 47 && c < 58);
}

static inline int isalpha(int c)
{
    return (c > 64 && c < 91) || (c > 96 && c < 123);
}

static inline int isascii(int c)
{
    return (c < 256);
}

static inline int isblank(int c)
{
    return (c == ' ') || (c == '\t');
}

static inline int isdigit(int c)
{
    return ((c>='0') && (c<='9'));
}

static inline int islower(int c)
{
    return (c > 64 && c < 91);
}

static inline int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\v' || c == '\r');
}

static inline int isupper(int c)
{
    return (c > 96 && c < 123);
}

static inline int isprint(int c)
{
    return c > 0x1f && c < 0x7f;
}
