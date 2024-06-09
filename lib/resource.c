#include <sys/resource.h>

int getpriority(int which, id_t who)
{
    NOT_IMPLEMENTED(-1 | which | who);
}

int getrlimit(int resource, struct rlimit* rlp)
{
    NOT_IMPLEMENTED(-1 | resource | ADDR(rlp));
}

int getrusage(int who, struct rusage* r_usage)
{
    NOT_IMPLEMENTED(-1 | who | ADDR(r_usage));
}

int setpriority(int which, id_t who, int value)
{
    NOT_IMPLEMENTED(-1 | which | who | value);
}

int setrlimit(int resource, const struct rlimit* rlp)
{
    NOT_IMPLEMENTED(-1 | resource | ADDR(rlp));
}
