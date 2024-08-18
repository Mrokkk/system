#include <string.h>
#include <sys/resource.h>

int LIBC(getpriority)(int which, id_t who)
{
    NOT_IMPLEMENTED(-1, "%d, %d", which, who);
}

int LIBC(getrlimit)(int resource, struct rlimit* rlp)
{
    NOT_IMPLEMENTED(-1, "%d, %p", resource, rlp);
}

int LIBC(getrusage)(int who, struct rusage* r_usage)
{
    memset(&r_usage->ru_stime, 0, sizeof(r_usage->ru_stime));
    memset(&r_usage->ru_utime, 0, sizeof(r_usage->ru_utime));
    NOT_IMPLEMENTED(-1, "%d, %p", who, r_usage);
}

int LIBC(setpriority)(int which, id_t who, int value)
{
    NOT_IMPLEMENTED(-1, "%d, %u, %d", which, who, value);
}

int LIBC(setrlimit)(int resource, const struct rlimit* rlp)
{
    NOT_IMPLEMENTED(-1, "%d, %p", resource, rlp);
}

LIBC_ALIAS(getpriority);
LIBC_ALIAS(getrlimit);
LIBC_ALIAS(getrusage);
LIBC_ALIAS(setpriority);
LIBC_ALIAS(setrlimit);
