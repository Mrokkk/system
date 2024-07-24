#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

char* LIBC(optarg);
int LIBC(optind) = 1, LIBC(opterr) = 1, LIBC(optopt);

enum order
{
    ORIGINAL,
    PERMUTATED,
    ORDERED,
};

typedef enum order order_t;

struct getopt_context
{
    int     optind;
    int     opterr;
    int     optopt;
    char*   optarg;
    int     first_nonopt;
    char*   nextchar;
    bool    initialized;
    order_t order;
};

typedef struct getopt_context getopt_context_t;

#define GETOPT_ERR(msg, ...) \
    do \
    { \
        if (opterr) fprintf(stderr, "%s: " msg "\n", argv[0], __VA_ARGS__); \
    } \
    while (0)

static void argv_shift(int argc, char* argv[], getopt_context_t* c)
{
    if (*argv[c->optind] == '-')
    {
        return;
    }

    int sum = 0;

    while (*argv[c->optind] != '-' && sum < argc - c->optind)
    {
        int optind_temp = c->optind;
        size_t count = 0;
        char* temp[32];

        for (; *argv[optind_temp] != '-' && count < ARRAY_SIZE(temp); count++)
        {
            temp[count] = argv[optind_temp++];
            if (optind_temp >= argc)
            {
                return;
            }
        }

        if (count)
        {
            sum += count;
            int optend = argc - count;
            c->first_nonopt = optend;
            memcpy(argv + c->optind, argv + c->optind + count, (optend - c->optind) * sizeof(char*));
            memcpy(argv + optend, temp, count * sizeof(char*));
        }
    }
}

static const char* getopt_initialize(int argc, char* const argv[], const char* optstring, getopt_context_t* c)
{
    UNUSED(argv);

    switch (*optstring)
    {
        case '-':
            optstring++;
            c->order = ORIGINAL;
            break;

        case '+':
            optstring++;
            c->order = ORDERED;
            break;

        default:
            c->order = !!getenv("POSIXLY_CORRECT") ? ORDERED : PERMUTATED;
            break;
    }

    c->optind = 1;
    c->optarg = NULL;
    c->first_nonopt = argc;
    c->nextchar = NULL;
    c->initialized = true;

    return optstring;
}

static inline void longindex_set(int* longindex, int value)
{
    if (longindex)
    {
        *longindex = value;
    }
}

static int long_option_found(const struct option* longopts, int i, int* longindex, getopt_context_t* c)
{
    int res;

    longindex_set(longindex, i);

    if (!longopts[i].flag)
    {
        res = longopts[i].val;
    }
    else
    {
        *longopts[i].flag = longopts[i].val;
        res = 0;
    }

    c->nextchar = NULL;
    return res;
}

static int getopt_impl(
    int argc,
    char* const argv[],
    const char* optstring,
    const struct option* longopts,
    int* longindex,
    bool longopts_only,
    getopt_context_t* c)
{
    c->optarg = NULL;

    if (UNLIKELY(argc < 2))
    {
        return -1;
    }

    if (!optind || !c->initialized)
    {
        optstring = getopt_initialize(argc, argv, optstring, c);
    }
    else if (*optstring == '-' || *optstring == '+')
    {
        optstring++;
    }

    if (c->optind != argc && (!c->nextchar || !*c->nextchar))
    {
        if (*argv[c->optind] != '-')
        {
            switch (c->order)
            {
                case ORIGINAL:
                    c->optarg = argv[c->optind++];
                    return 1;

                case PERMUTATED:
                    argv_shift(argc, (char**)argv, c);
                    FALLTHROUGH;

                case ORDERED:
                    if (*argv[c->optind] != '-')
                    {
                        c->optind++;
                        return -1;
                    }
            }
        }
        else if (!strcmp("--", argv[c->optind]))
        {
            c->optind++;
            return -1;
        }

        c->nextchar = argv[c->optind++] + 1;
    }

    if (!c->nextchar || !*c->nextchar)
    {
        goto no_more_opts;
    }

    if (longopts && (longopts_only || (c->nextchar[0] == '-' && c->nextchar[1])))
    {
        if (*c->nextchar == '-')
        {
            c->nextchar++;
        }

        for (int i = 0; longopts[i].name; ++i)
        {
            size_t len = strlen(longopts[i].name);

            if (strncmp(longopts[i].name, c->nextchar, len))
            {
                continue;
            }

            if (c->nextchar[len] && c->nextchar[len] != '=')
            {
                continue;
            }

            if (longopts[i].has_arg == required_argument || longopts[i].has_arg == optional_argument)
            {
                if (c->nextchar[len] == '=')
                {
                    c->optarg = c->nextchar + len + 1;
                    return long_option_found(longopts, i, longindex, c);
                }
                else if (c->optind >= c->first_nonopt)
                {
                    if (longopts[i].has_arg != optional_argument)
                    {
                        GETOPT_ERR("option requires an argument -- \'%s\'", longopts[i].name);

                        c->nextchar = NULL;
                        return '?';
                    }
                    else
                    {
                        return long_option_found(longopts, i, longindex, c);
                    }
                }
                else
                {
                    c->optarg = argv[c->optind++];
                    return long_option_found(longopts, i, longindex, c);
                }
            }
            else
            {
                return long_option_found(longopts, i, longindex, c);
            }
        }

        if (!longopts_only)
        {
            goto invalid_options;
        }
    }

    for (int i = 0; optstring[i]; ++i)
    {
        if (*c->nextchar != optstring[i])
        {
            continue;
        }

        if (optstring[i + 1] != ':')
        {
            return *c->nextchar++;
        }

        int res = *c->nextchar;

        if (c->nextchar[1])
        {
            c->optarg = c->nextchar + 1;
            c->nextchar = NULL;
            return res;
        }
        else if (c->optind == c->first_nonopt)
        {
            if (optstring[i + 2] == ':')
            {
                c->nextchar = NULL;
                return *optstring;
            }

            GETOPT_ERR("option requires an argument -- \'%c\'", optstring[i]);

            c->nextchar = NULL;
            return '?';
        }
        else
        {
            c->optarg = argv[c->optind++];
            c->nextchar = NULL;
            return res;
        }
    }

invalid_options:
    GETOPT_ERR("invalid option -- \'%c\'", *c->nextchar);

    c->optopt = *c->nextchar++;

    return '?';

no_more_opts:
    c->nextchar = NULL;
    return -1;
}

static getopt_context_t getopt_context;

int LIBC(getopt)(int argc, char* const argv[], const char* optstring)
{
    getopt_context.optind = optind;
    getopt_context.opterr = opterr;

    int res = getopt_impl(argc, argv, optstring, NULL, NULL, false, &getopt_context);

    optind = getopt_context.optind;
    optopt = getopt_context.optopt;
    optarg = getopt_context.optarg;

    return res;
}

int LIBC(getopt_long)(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex)
{
    getopt_context.optind = optind;
    getopt_context.opterr = opterr;

    int res = getopt_impl(argc, argv, optstring, longopts, longindex, false, &getopt_context);

    optind = getopt_context.optind;
    optopt = getopt_context.optopt;
    optarg = getopt_context.optarg;

    return res;
}

int LIBC(getopt_long_only)(
    int argc,
    char* const* argv,
    const char* optstring,
    const struct option* longopts,
    int* longindex)
{
    getopt_context.optind = optind;
    getopt_context.opterr = opterr;

    int res = getopt_impl(argc, argv, optstring, longopts, longindex, true, &getopt_context);

    optind = getopt_context.optind;
    optopt = getopt_context.optopt;
    optarg = getopt_context.optarg;

    return res;
}

LIBC_ALIAS(optarg);
LIBC_ALIAS(optind);
LIBC_ALIAS(opterr);
LIBC_ALIAS(optopt);
LIBC_ALIAS(getopt);
LIBC_ALIAS(getopt_long);
LIBC_ALIAS(getopt_long_only);
