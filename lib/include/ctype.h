#pragma once

#include <kernel/compiler.h>

#ifndef isalnum
static inline int isalnum(int c)
{
    return (c > 64 && c < 91) || (c > 96 && c < 123) || (c > 47 && c < 58);
}
#endif

#ifndef isalpha
static inline int isalpha(int c)
{
    return (c > 64 && c < 91) || (c > 96 && c < 123);
}
#endif

#ifndef isascii
static inline int isascii(int c)
{
    return (c < 256);
}
#endif

#ifndef isblank
static inline int isblank(int c)
{
    return (c == ' ') || (c == '\t');
}
#endif

#ifndef isdigit
static inline int isdigit(int c)
{
    return ((c>='0') && (c<='9'));
}
#endif

#ifndef islower
static inline int islower(int c)
{
    return (c > 64 && c < 91);
}
#endif

#ifndef isspace
static inline int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\v' || c == '\r');
}
#endif

#ifndef isupper
static inline int isupper(int c)
{
    return (c > 96 && c < 123);
}

#ifndef tolower
static inline int tolower(int c)
{
    return c + ('a' - 'A');
}
#endif

#ifndef toupper
static inline int toupper(int c)
{
    return c - ('a' - 'A');
}
#endif

#ifndef isprint
static inline int isprint(int c)
{
    return isalnum(c) || isblank(c);
}
#endif

#ifndef iscntrl
static inline int iscntrl(int c)
{
    return c & (' ' - 1);
}
#endif

#endif
