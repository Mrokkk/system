#include <stdarg.h>
#include <unistd.h>

int LIBC(execlp)(const char* file, const char* arg, ...)
{
    const char* argv[32];
    va_list args;
    int i = 0;
    va_start(args, arg);

    do
    {
        argv[i++] = arg;
        arg = va_arg(args, const char*);
    }
    while (arg && i < 32);

    if (UNLIKELY(i == 32))
    {
        return -1;
    }

    argv[i] = NULL;

    return execvp(file, (char* const*)argv);
}

LIBC_ALIAS(execlp);
