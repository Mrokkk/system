#pragma once

#define _U 01
#define _L 02
#define _N 04
#define _S 010
#define _P 020
#define _C 040
#define _X 0100
#define _B 0200

extern char const _ctype_[257] __attribute__((visibility("default")));

#undef isalnum
static inline int isalnum(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_U | _L | _N);
}

#undef isalpha
static inline int isalpha(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_U | _L);
}

#undef isascii
static inline int isascii(int c)
{
    return c < 128;
}

#undef isblank
static inline int isblank(int c)
{
    return (c == ' ') || (c == '\t');
}

#undef isdigit
static inline int isdigit(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_N);
}

#undef islower
static inline int islower(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_L);
}

#undef isspace
static inline int isspace(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_S);
}

#undef isupper
static inline int isupper(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_U);
}

#undef tolower
static inline int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c | 0x20;
    }
    return c;
}

#undef toupper
static inline int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
    {
        return c & ~0x20;
    }
    return c;
}

#undef iscntrl
static inline int iscntrl(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_C);
}

#undef ispunct
static inline int ispunct(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_P);
}

#undef isprint
static inline int isprint(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_P | _U | _L | _N | _B);
}

#undef isxdigit
static inline int isxdigit(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_N | _X);
}

#undef isgraph
static inline int isgraph(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_P | _U | _L | _N);
}

#define iswgraph isgraph
