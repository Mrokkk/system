#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/ctype.h>
#include <kernel/string.h>
#include <kernel/kernel.h>

int sprintf(char* buf, const char* fmt, ...);

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
        __res = ((unsigned long) n) % (unsigned) base; \
        n = ((unsigned long) n) / (unsigned) base; \
        __res; \
    })

// we are called with base 8, 10 or 16, only, thus don't need "G..."
static const char digits[16] = "0123456789ABCDEF";

static char* number(
    char* str,
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
        return NULL;
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
        for (; size-- > 0; *str++ = ' ');
    }

    if (sign)
    {
        *str++ = sign;
    }

    if (type & SPECIAL)
    {
        if (base == 8)
        {
            *str++ = '0';
        }
        else if (base == 16)
        {
            *str++ = '0';
            *str++ = ('X' | locase);
        }
    }

    if (!(type & LEFT))
    {
        for (; size-- > 0; *str++ = c);
    }

    while (i < precision--)
    {
        *str++ = '0';
    }

    while (i-- > 0)
    {
        *str++ = tmp[i];
    }

    while (size-- > 0)
    {
        *str++ = ' ';
    }
    return str;
}

int vsprintf(char* buf, const char* fmt, va_list args)
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
            *str++ = *fmt;
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
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
        {
            qualifier = *fmt++;
            if (*fmt == 'l')
            {
                qualifier = qualifier << 8 | 'l';
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
                        *str++ = ' ';
                    }
                }
                *str++ = (unsigned char)va_arg(args, int);
                while (--field_width > 0)
                {
                    *str++ = ' ';
                }
                continue;

            case 's':
                s = va_arg(args, char*);
                len = strnlen(s, precision);

                if (!(flags & LEFT))
                {
                    while (len < field_width--)
                    {
                        *str++ = ' ';
                    }
                }
                for (i = 0; i < len; ++i)
                {
                    *str++ = *s++;
                }
                while (len < field_width--)
                {
                    *str++ = ' ';
                }
                continue;

            case 'p':
                if (field_width == -1)
                {
                    field_width = 2 * sizeof(void*);
                    flags |= ZEROPAD;
                }
                str = number(
                    str,
                    (unsigned long)va_arg(args, void*),
                    16,
                    field_width,
                    precision,
                    flags);
                continue;

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
                *str++ = '%';
                continue;

            case 'b':
                *str++ = '0';
                *str++ = 'b';
                base = 2;
                break;

            case 'B':
            {
                int b = va_arg(args, int);
                str += sprintf(str, b ? "true" : "false");
                continue;
            }

                // integer number formats - set up the flags and "break"
            case 'o':
                base = 8;
                break;

            case 'x':
                *str++ = '0';
                *str++ = 'x';
                fallthrough;
            case 'X':
                base = 16;
                flags |= SMALL;
                break;

            case 'd':
            case 'i':
                flags |= SIGN;
            case 'u':
                break;

            case 'S':
                s = va_arg(args, char*);
                len = strnlen(s, precision);
                str += sprintf(str, "string{\"%s\", len=%u, ptr=%x}", s, len, (uint32_t)s);
                continue;

            case 'O':
            {
                const magic_t* magic = va_arg(args, magic_t*);
                if (magic == NULL)
                {
                    str = strcpy(str, "<null>") - 1;
                    continue;
                }
                else
                {
                    str += sprintf(str, "<%x>", (uint32_t)magic);
                    continue;
                }
            }

            default:
                *str++ = '%';
                if (*fmt)
                {
                    *str++ = *fmt;
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
        else if (qualifier == ('l' | 'l' << 8))
        {
            uint64_t num = va_arg(args, uint64_t);
            str = number(str, (uint32_t)(num >> 32), base, field_width, precision, flags);
            str = number(str, (uint32_t)(num & ~0UL), base, field_width, precision, flags);
            goto next;
        }
        else
        {
            num = va_arg(args, unsigned int);
        }

        str = number(str, num, base, field_width, precision, flags);
next:
    }

    *str = '\0';
    return str - buf;
}

int sprintf(char* buf, const char* fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

    return i;
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
