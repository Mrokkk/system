#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define PRINTF_BUFFER   512

struct file
{
    int fd;
    uint32_t magic;
    bool error;
};

static FILE files[] = {
    { STDIN_FILENO, FILE_MAGIC, false },
    { STDOUT_FILENO, FILE_MAGIC, false },
    { STDERR_FILENO, FILE_MAGIC, false }
};

FILE* stdin = &files[STDIN_FILENO];
FILE* stdout = &files[STDOUT_FILENO];
FILE* stderr = &files[STDERR_FILENO];

static inline int skip_atoi(const char **s)
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

int scanf(const char*, ...)
{
    NOT_IMPLEMENTED(EOF);
}

int fscanf(FILE*, const char*, ...)
{
    NOT_IMPLEMENTED(EOF);
}

int vscanf(const char*, va_list)
{
    NOT_IMPLEMENTED(EOF);
}

int vfscanf(FILE*, const char*, va_list)
{
    NOT_IMPLEMENTED(EOF);
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
                    flags |= ZEROPAD | SMALL | SPECIAL;
                }
                str = number(
                    str,
                    va_arg(args, unsigned long),
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
                base = 2;
                break;

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
        else
        {
            num = va_arg(args, unsigned int);
        }
        str = number(str, num, base, field_width, precision, flags);
    }

    *str = '\0';
    return str - buf;
}

int sprintf(char* buf, const char* fmt, ...)
{
    va_list args;
    int i;

    VALIDATE_INPUT(buf && fmt, -1);

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

    return i;
}

int printf(const char* fmt, ...)
{
    char printf_buf[PRINTF_BUFFER];
    va_list args;
    int printed;

    VALIDATE_INPUT(fmt, -1);

    va_start(args, fmt);
    printed = vsprintf(printf_buf, fmt, args);
    va_end(args);

    return write(1, printf_buf, printed);
}

int fprintf(FILE* file, const char* fmt, ...)
{
    char printf_buf[PRINTF_BUFFER];
    va_list args;
    int printed;

    VALIDATE_INPUT(file && fmt, -1);

    va_start(args, fmt);
    printed = vsprintf(printf_buf, fmt, args);
    va_end(args);

    return write(file->fd, printf_buf, printed);
}

FILE* fopen(const char* pathname, const char* mode)
{
    int fd;
    FILE* file;

    VALIDATE_INPUT(pathname && mode, NULL);
    fd = SAFE_SYSCALL(open(pathname, O_RDONLY), NULL); // FIXME: parse mode
    file = SAFE_ALLOC(malloc(sizeof(*file)), NULL);

    file->fd = fd;
    file->magic = FILE_MAGIC;

    return file;
}

FILE* fdopen(int fd, const char* mode)
{
    FILE* file;
    VALIDATE_INPUT(mode, NULL);
    file = SAFE_ALLOC(malloc(sizeof(*file)), NULL);
    file->fd = fd;
    file->magic = FILE_MAGIC;
    return file;
}

int fclose(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);
    return close(stream->fd);
}

int fileno(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);
    return stream->fd;
}

int fgetc(FILE* stream)
{
    char data[2];
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);
    SAFE_SYSCALL(read(stream->fd, data, 1), EOF);
    return (int)data[0];
}

char* fgets(char*, int, FILE*)
{
    NOT_IMPLEMENTED(NULL);
}

int ungetc(int, FILE*)
{
    NOT_IMPLEMENTED(EOF);
}

int fputc(int c, FILE* stream)
{
    SAFE_SYSCALL(write(stream->fd, (const char*)&c, 1), EOF);
    return c;
}

STRONG_ALIAS(fputc, putc);

int putchar(int c)
{
    return fputc(c, stdout);
}

int fputs(const char* s, FILE* file)
{
    VALIDATE_INPUT(FILE_CHECK(file) && s, EOF);
    return SAFE_SYSCALL(write(file->fd, s, strlen(s)), EOF);
}

int puts(const char* s)
{
    int len = fputs(s, stdout);
    SAFE_SYSCALL(write(stdout->fd, "\n", 1), EOF);
    return len + 1;
}

void clearerr(FILE* stream)
{
    if (stream)
    {
        stream->error = false;
    }
}

int feof(FILE*)
{
    return 0;
}

int ferror(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), 1);
    return stream->error;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream) && ptr, 0);
    int count = read(stream->fd, ptr, size * nmemb);

    if (UNLIKELY(count < 0))
    {
        stream->error = true;
        return 0;
    }

    return count;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream) && ptr, 0);
    int count = write(stream->fd, ptr, size * nmemb);

    if (UNLIKELY(count < 0))
    {
        stream->error = true;
        return 0;
    }

    return count;
}

int fflush(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);
    return 0;
}
