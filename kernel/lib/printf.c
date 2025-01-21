#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/ctype.h>
#include <kernel/string.h>
#include <kernel/kernel.h>

static inline int skip_atoi(const char** s)
{
    int i = 0;

    while (isdigit((int)**s))
    {
        i = i * 10 + *((*s)++) - '0';
    }
    return i;
}

#define ZEROPAD 1   // pad with zero
#define SIGN    2   // unsigned/signed long
#define PLUS    4   // show plus
#define SPACE   8   // space if plus
#define LEFT    16  // left justified
#define SMALL   32  // Must be 32 == 0x20
#define SPECIAL 64  // 0x

// we are called with base 8, 10 or 16, only, thus don't need "G..."
static const char digits[16] = "0123456789ABCDEF";

#define PUTC(c) \
    ({ if (likely(str < end)) { *str++ = c; }; 0; })

#define PUTS(s) \
    ({ const char* _s = s; while (*_s) { PUTC(*_s); ++_s; }; 0; })

#define NUMBER_TYPE long
#define NUMBER_SUFFIX l
#include "number.c"
#undef NUMBER_TYPE
#undef NUMBER_SUFFIX

#if BITS_PER_LONG == 64

#define numberll numberl

#else

#define NUMBER_TYPE long long
#define NUMBER_SUFFIX ll
#include "number.c"

#endif

static int vsnprintf_impl(char* buf, const char* end, const char* fmt, va_list args)
{
    int len;
    unsigned long num;
    int i, base;
    char* str;
    const char* s;

    int flags;          // flags to number()
    int field_width;    // width of output field
    int precision;      // min. # of digits for integers; max number of chars for from string
    int qualifier;      // 'h', 'l', or 'L' for integer fields

    for (str = buf; *fmt; ++fmt)
    {
        if (*fmt != '%')
        {
            PUTC(*fmt);
            continue;
        }

        flags = 0; // process flags

    repeat:
        ++fmt;  // this also skips first '%'
        switch (*fmt)
        {
            case '-':
                flags |= LEFT;
                goto repeat;
            case '+':
                flags |= PLUS;
                goto repeat;
            case ' ':
                flags |= SPACE;
                goto repeat;
            case '#':
                flags |= SPECIAL;
                goto repeat;
            case '0':
                flags |= ZEROPAD;
                goto repeat;
        }

        // get field width
        field_width = -1;
        if (isdigit((int)*fmt))
        {
            field_width = skip_atoi(&fmt);
        }
        else if (*fmt == '*')
        {
            ++fmt;
            // it's the next argument
            field_width = va_arg(args, int);
            if (field_width < 0)
            {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        // get the precision
        precision = -1;
        if (*fmt == '.')
        {
            ++fmt;
            if (isdigit((int)*fmt))
            {
                precision = skip_atoi(&fmt);
            }
            else if (*fmt == '*')
            {
                ++fmt;
                // it's the next argument
                precision = va_arg(args, int);
            }
            if (precision < 0)
            {
                precision = 0;
            }
        }

        // get the conversion qualifier
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt == 'z')
        {
            qualifier = *fmt++;
            if (*fmt == 'l')
            {
                qualifier = 'L';
                ++fmt;
            }
        }

        // default base
        base = 10;

        switch (*fmt)
        {
            case 'c':
                if (!(flags & LEFT))
                {
                    while (--field_width > 0)
                    {
                        PUTC(' ');
                    }
                }

                PUTC((char)va_arg(args, int));

                while (--field_width > 0)
                {
                    PUTC(' ');
                }
                continue;

            case 's':
                s = va_arg(args, char*);

                if (unlikely(!s))
                {
                    PUTS("(null)");
                    continue;
                }

                len = strnlen(s, precision);

                if (!(flags & LEFT))
                {
                    while (len < field_width--)
                    {
                        PUTC(' ');
                    }
                }
                for (i = 0; i < len; ++i)
                {
                    PUTC(*s++);
                }
                while (len < field_width--)
                {
                    PUTC(' ');
                }
                continue;

            case 'p':
                flags |= ZEROPAD | SMALL | SPECIAL;
                base = 16;
                break;

            case 'n':
                if (qualifier == 'l')
                {
                    long* ip = va_arg(args, long*);
                    *ip = (str - buf);
                }
                else
                {
                    int* ip = va_arg(args, int*);
                    *ip = (str - buf);
                }
                continue;

            case '%':
                PUTC('%');
                continue;

            case 'b':
                PUTC('0');
                PUTC('b');
                base = 2;
                break;

            case 'B':
            {
                int b = va_arg(args, int);
                PUTS(b ? "true" : "false");
                continue;
            }

            // integer number formats - set up the flags and "break"
            case 'o':
                base = 8;
                break;

            case 'x':
                flags |= SMALL;
                fallthrough;
            case 'X':
                base = 16;
                break;

            case 'd':
            case 'i':
                flags |= SIGN;
            case 'u':
                break;

            case 'S':
                s = va_arg(args, char*);
                len = strnlen(s, precision);
                str += snprintf(str, end - str, "string{\"%s\", len=%u, ptr=%p}", s, len, s);
                continue;

            case 'O':
            {
                const magic_t* magic = va_arg(args, magic_t*);
                if (magic == NULL)
                {
                    str += snprintf(str, end - str, "(null)");
                    continue;
                }
                else
                {
                    str += snprintf(str, end - str, "<%p>", magic);
                    continue;
                }
            }

            default:
                PUTC('%');
                if (*fmt)
                {
                    PUTC(*fmt);
                }
                else
                {
                    --fmt;
                }
                continue;
        }

        switch (qualifier)
        {
            case 'l':
                num = flags & SIGN
                    ? (unsigned long)va_arg(args, long)
                    : va_arg(args, unsigned long);
                break;
            case 'z':
                num = flags & SIGN
                    ? (unsigned long)va_arg(args, ssize_t)
                    : (unsigned long)va_arg(args, size_t);
                break;
            case 'L':
            {
                uint64_t num = va_arg(args, uint64_t);
                str = numberll(str, num, base, field_width, precision, flags, end);
                goto next;
            }
            case 'h':
            default:
                num = flags & SIGN
                    ? (unsigned long)va_arg(args, int)
                    : (unsigned long)va_arg(args, unsigned int);
                break;
        }

        str = numberl(str, num, base, field_width, precision, flags, end);
next:
    }

    if (unlikely(str == end))
    {
        *(str - 1) = '\0';
    }
    else
    {
        *str = '\0';
    }

    return str - buf;
}

int snprintf(char* buf, size_t size, const char* fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsnprintf_impl(buf, buf + size, fmt, args);
    va_end(args);

    return i;
}

char* csnprintf(char* buf, const char* end, const char* fmt, ...)
{
    va_list args;
    int i;

    if (unlikely(buf == end))
    {
        return buf;
    }

    va_start(args, fmt);
    i = vsnprintf_impl(buf, end, fmt, args);
    va_end(args);

    return buf + i;
}

char* ssnprintf(char* buf, size_t size, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf_impl(buf, buf + size, fmt, args);
    va_end(args);

    return buf;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args)
{
    return vsnprintf_impl(buf, buf + size, fmt, args);
}

int strtoi(const char* str)
{
    int res = 0;

    while (*str)
    {
        if (*str < '0' || *str > '9')
        {
            goto next;
        }

        res *= 10;
        res += *str - '0';
next:
        ++str;
    }

    return res;
}
