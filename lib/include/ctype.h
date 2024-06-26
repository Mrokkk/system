#pragma once

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
#endif

#ifndef tolower
static inline int tolower(int c)
{
    return islower(c)
        ? c + ('a' - 'A')
        : c;
}
#endif

#ifndef toupper
static inline int toupper(int c)
{
    return isupper(c)
        ? c - ('a' - 'A')
        : c;
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
    return c < 0x20 || c == 0x7f;
}
#endif

#ifndef ispunct
static inline int ispunct(int c)
{
    const char* chars = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    for (unsigned i = 0; i < __builtin_strlen(chars); ++i)
    {
        if (c == chars[i])
        {
            return 1;
        }
    }
    return 0;
}
#endif

#ifndef isxdigit
static inline int isxdigit(int c)
{
    const char* chars = "0123456789abcdefABCDEF";
    for (unsigned i = 0; i < __builtin_strlen(chars); ++i)
    {
        if (c == chars[i])
        {
            return 1;
        }
    }
    return 0;
}
#endif

#ifndef isgraph
static inline int isgraph(int c)
{
    return c > 0x1f && c < 0x7f;
}
#endif

#define iswgraph isgraph
