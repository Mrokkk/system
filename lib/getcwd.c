#include <stddef.h>

extern int LIBC(getcwd)(char*, size_t);

char* getcwd(char* buf, size_t size)
{
    VALIDATE_INPUT(buf && size, NULL);
    SAFE_SYSCALL(LIBC(getcwd)(buf, size), NULL);
    return buf;
}
