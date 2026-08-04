#pragma once

#include <common/bits.h>

__BEGIN_DECLS

#define _U 01
#define _L 02
#define _N 04
#define _S 010
#define _P 020
#define _C 040
#define _X 0100
#define _B 0200

extern char const _ctype_[257] __attribute__((visibility("default")));

static inline int __inline_isalnum(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_U | _L | _N);
}

static inline int __inline_isalpha(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_U | _L);
}

static inline int __inline_isascii(int c)
{
    return c < 128;
}

static inline int __inline_isblank(int c)
{
    return (c == ' ') || (c == '\t');
}

static inline int __inline_isdigit(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_N);
}

static inline int __inline_islower(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_L);
}

static inline int __inline_isspace(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_S);
}

static inline int __inline_isupper(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_U);
}

static inline int __inline_iscntrl(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_C);
}

static inline int __inline_ispunct(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_P);
}

static inline int __inline_isprint(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_P | _U | _L | _N | _B);
}

static inline int __inline_isxdigit(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_N | _X);
}

static inline int __inline_isgraph(int c)
{
    return _ctype_[(unsigned char)(c) + 1] & (_P | _U | _L | _N);
}

#define iswgraph isgraph

static inline int __inline_tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c | 0x20;
    }
    return c;
}

static inline int __inline_toupper(int c)
{
    if (c >= 'a' && c <= 'z')
    {
        return c & ~0x20;
    }
    return c;
}

#ifdef __cplusplus

int isalnum(int c);
int isalpha(int c);
int isascii(int c);
int iscntrl(int c);
int isdigit(int c);
int isxdigit(int c);
int isspace(int c);
int ispunct(int c);
int isprint(int c);
int isgraph(int c);
int islower(int c);
int isupper(int c);
int isblank(int c);
int toascii(int c);
int tolower(int c);
int toupper(int c);

#else

#define isalnum     __inline_isalnum
#define isalpha     __inline_isalpha
#define isascii     __inline_isascii
#define iscntrl     __inline_iscntrl
#define isdigit     __inline_isdigit
#define isxdigit    __inline_isxdigit
#define isspace     __inline_isspace
#define ispunct     __inline_ispunct
#define isprint     __inline_isprint
#define isgraph     __inline_isgraph
#define islower     __inline_islower
#define isupper     __inline_isupper
#define isblank     __inline_isblank
#define toascii     __inline_toascii
#define tolower     __inline_tolower
#define toupper     __inline_toupper

#endif

__END_DECLS
