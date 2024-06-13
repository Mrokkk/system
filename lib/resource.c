#include <sys/resource.h>

int getpriority(int which, id_t who)
{
    NOT_IMPLEMENTED(-1, "%d, %d", which, who);
}

int getrlimit(int resource, struct rlimit* rlp)
{
    NOT_IMPLEMENTED(-1, "%d, %p", resource, rlp);
}

int getrusage(int who, struct rusage* r_usage)
{
    NOT_IMPLEMENTED(-1, "%d, %p", who, r_usage);
}

int setpriority(int which, id_t who, int value)
{
    NOT_IMPLEMENTED(-1, "%d, %u, %d", which, who, value);
}

int setrlimit(int resource, const struct rlimit* rlp)
{
    NOT_IMPLEMENTED(-1, "%d, %p", resource, rlp);
}
