#include <getopt.h>

char* optarg;
int optind, opterr, optopt;

int LIBC(getopt_long)(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex)
{
    NOT_IMPLEMENTED(-1, "%d, %p, \"%s\", %p, %p",
        argc, argv, optstring, longopts, longindex);
}

int LIBC(getopt_long_only)(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex)
{
    NOT_IMPLEMENTED(-1, "%d, %p, \"%s\", %p, %p",
        argc, argv, optstring, longopts, longindex);
}

LIBC_ALIAS(getopt_long);
LIBC_ALIAS(getopt_long_only);
