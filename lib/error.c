#include <error.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

unsigned int LIBC(error_message_count);
int LIBC(error_one_per_line);
void (*LIBC(error_print_progname))(void);

LIBC_ALIAS(error_message_count);
LIBC_ALIAS(error_one_per_line);
LIBC_ALIAS(error_print_progname);

static void error_impl(
    int status,
    int errnum,
    const char* filename,
    unsigned int linenum,
    const char* format,
    va_list args)
{
    fprintf(stderr, "%s:", program_invocation_name);

    filename
        ? fprintf(stderr, "%s:%u ", filename, linenum)
        : putc(' ', stderr);

    vfprintf(stderr, format, args);

    if (errnum)
    {
        fprintf(stderr, ": %s", strerror(errnum));
    }

    putc('\n', stderr);

    if (status)
    {
        exit(status);
    }
}

void LIBC(error)(int status, int errnum, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    error_impl(status, errnum, NULL, 0, format, args);
    va_end(args);
}

void LIBC(error_at_line)(
    int status,
    int errnum,
    const char* filename,
    unsigned int linenum,
    const char* format, ...)
{
    const char* short_file = strrchr(filename, '/');
    short_file = short_file ? short_file + 1 : filename;

    va_list args;
    va_start(args, format);
    error_impl(status, errnum, short_file, linenum, format, args);
    va_end(args);
}

LIBC_ALIAS(error);
LIBC_ALIAS(error_at_line);
