#include <getopt.h>

char* optarg;
int optind, opterr, optopt;

int getopt_long(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex)
{
    UNUSED(argc); UNUSED(argv); UNUSED(optstring);
    UNUSED(longopts); UNUSED(longindex);
    return -1;
}

int getopt_long_only(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex)
{
    UNUSED(argc); UNUSED(argv); UNUSED(optstring);
    UNUSED(longopts); UNUSED(longindex);
    return -1;
}
