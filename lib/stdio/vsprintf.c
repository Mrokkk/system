#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "printf_buffer.h"
#include "printf_buffer_string.h"

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

#define __do_div(n, base) \
    ({ \
        int __res; \
        __res = ((unsigned long)n) % (unsigned)base; \
        n = ((unsigned long)n) / (unsigned)base; \
        __res; \
    })

// we are called with base 8, 10 or 16, only, thus don't need "G..."
static const char digits[16] = "0123456789ABCDEF";

#define PUTC(c) ({ if (UNLIKELY(buffer->putc(buffer, c))) return -1; 0; })
#define PUTS(s) ({ const char* __str = s; for (; *__str; PUTC(*__str++)); 0; })

static int number(
    printf_buffer_t* buffer,
    long num,
    int base,
    int size,
    int precision,
    int type)
{
    char tmp[66];
    char c, sign, locase;
    int i;

    // locase = 0 or 0x20. ORing digits or letters with 'locase'
    // produces same digits or (maybe lowercased) letters
    locase = (type & SMALL);
    if (type & LEFT)
    {
        type &= ~ZEROPAD;
    }
    if (base < 2 || base > 16)
    {
        return -1;
    }

    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;

    if (type & SIGN)
    {
        if (num < 0)
        {
            sign = '-';
            num = -num;
            size--;
        }
        else if (type & PLUS)
        {
            sign = '+';
            size--;
        }
        else if (type & SPACE)
        {
            sign = ' ';
            size--;
        }
    }

    if (type & SPECIAL)
    {
        if (base == 16)
        {
            size -= 2;
        }
        else if (base == 8)
        {
            size--;
        }
    }

    i = 0;
    if (num == 0)
    {
        tmp[i++] = '0';
    }
    else
    {
        for (; num != 0; tmp[i++] = (digits[__do_div(num, base)] | locase));
    }

    if (i > precision)
    {
        precision = i;
    }

    size -= precision;
    if (!(type & (ZEROPAD + LEFT)))
    {
        for (; size-- > 0; PUTC(' '));
    }

    if (sign)
    {
        PUTC(sign);
    }

    if (type & SPECIAL)
    {
        if (base == 8)
        {
            PUTC('0');
        }
        else if (base == 16)
        {
            PUTC('0');
            PUTC('X' | locase);
        }
    }

    if (!(type & LEFT))
    {
        for (; size-- > 0; PUTC(c));
    }

    while (i < precision--)
    {
        PUTC('0');
    }

    while (i-- > 0)
    {
        PUTC(tmp[i]);
    }

    while (size-- > 0)
    {
        PUTC(' ');
    }
    return 0;
}

int vsprintf_internal(printf_buffer_t* buffer, const char* fmt, va_list args)
{
    int len;
    unsigned long num;
    int i, base;
    const char* s;

    int flags;          // flags to number()
    int field_width;    // width of output field
    int precision;      // min. # of digits for integers; max number of chars for from string
    int qualifier;      // 'h', 'l', or 'L' for integer fields

    for (; *fmt; ++fmt)
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
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt == 'j')
        {
            qualifier = *fmt;
            ++fmt;
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

                if (UNLIKELY(s == NULL))
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
                if (field_width == -1)
                {
                    field_width = 2 * sizeof(void*);
                    flags |= ZEROPAD | SMALL | SPECIAL;
                }
                if (UNLIKELY(number(
                    buffer,
                    va_arg(args, unsigned long),
                    16,
                    field_width,
                    precision,
                    flags)))
                {
                    return -1;
                }
                continue;

            case 'n':
                if (qualifier == 'l')
                {
                    long* ip = va_arg(args, long*);
                    *ip = buffer->current - buffer->start;
                }
                else
                {
                    int* ip = va_arg(args, int*);
                    *ip = buffer->current - buffer->start;
                }
                continue;

            case '%':
                PUTC('%');
                continue;

            case 'b':
                base = 2;
                break;

                // integer number formats - set up the flags and "break"
            case 'o':
                base = 8;
                break;

            case 'x':
                flags |= SMALL;
                FALLTHROUGH;
            case 'X':
                base = 16;
                break;

            case 'd':
            case 'i':
                flags |= SIGN;
            case 'u':
                break;

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

        if (qualifier == 'l')
        {
            num = va_arg(args, unsigned long);
        }
        else if (qualifier == 'h')
        {
            num = (unsigned short)va_arg(args, int);
            if (flags & SIGN)
            {
                num = (short)num;
            }
        }
        else if (flags & SIGN)
        {
            num = va_arg(args, int);
        }
        else
        {
            num = va_arg(args, unsigned int);
        }
        if (UNLIKELY(number(buffer, num, base, field_width, precision, flags)))
        {
            return -1;
        }
    }

    return buffer->current - buffer->start;
}

int LIBC(vsprintf)(char* buf, const char* fmt, va_list args)
{
    int res;

    printf_buffer_t buffer = {
        .start = buf,
        .end = (char*)0xffffffff,
        .current = buf,
        .putc = &string_putc_no_bound,
    };

    res = vsprintf_internal(&buffer, fmt, args);

    if (UNLIKELY(res == -1))
    {
        return res;
    }

    string_putc_no_bound(&buffer, 0);

    return res;
}

int LIBC(vsnprintf)(char* buf, size_t size, const char* fmt, va_list args)
{
    int res;

    printf_buffer_t buffer = {
        .start = buf,
        .end = buf + size,
        .current = buf,
        .putc = &string_putc,
    };

    res = vsprintf_internal(&buffer, fmt, args);

    // FIXME: if output is truncated this should return number of bytes which
    // would have been written if there's no limit
    if (UNLIKELY(res == -1))
    {
        return res;
    }

    if (res < (int)size)
    {
        string_putc(&buffer, 0);
    }

    return res;
}

LIBC_ALIAS(vsprintf);
LIBC_ALIAS(vsnprintf);
