#pragma once

#include <unistd.h>

int getopt(int argc, char* const* argv, const char* optstring);

extern char* optarg;
extern int optind, opterr, optopt;

struct option
{
    char const* name;
    int         has_arg;
    int*        flag;
    int         val;
};

int getopt_long(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex);

int getopt_long_only(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex);
