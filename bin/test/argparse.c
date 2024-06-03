#include "argparse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

static const char* app_name_get(const char* app_name)
{
    char* temp = strrchr(app_name, '/');
    return temp ? temp : app_name;
}

static void strcpy_upper_case(char* dest, const char* src)
{
    while ((*dest++ = toupper(*src++)));
    *dest = 0;
}

static void opt_print(option_t* opt)
{
    char buf[256];
    char* it = buf;
    bool has_long = !!opt->long_v;
    bool has_short = !!opt->short_v;
    bool has_both = has_long && has_short;

    buf[0] = 0;

    switch (opt->type)
    {
        case OPT_BOOL:
            sprintf(buf,
                "%s%c %s",
                has_short ? opt->short_v : "",
                has_both ? ',' : ' ',
                has_long ? opt->long_v : "");
            break;
        case OPT_VALUE:
            it += sprintf(it,
                "%s%c ",
                has_short ? opt->short_v : "",
                has_both ? ',' : ' ');

            if (has_long)
            {
                it += sprintf(it, "%s=", opt->long_v);
                strcpy_upper_case(it, opt->long_v + 2);
            }
            break;
        case OPT_POSITIONAL:
            // TODO
            break;
    }

    printf("  %-24s %s\n", buf, opt->description);
}

static void help_print(const char* app_name, option_t* options, size_t options_len)
{
    printf("Usage: %s [OPTION]...\n"
           "\n"
           "Options:\n", app_name_get(app_name));

    for (size_t i = 0; i < options_len; ++i)
    {
        opt_print(&options[i]);
    }
    putchar('\n');
}

static bool arg_parse(
    char* arg,
    size_t arg_len,
    option_t* options,
    size_t options_len,
    void* user_data,
    option_t** waiting_option)
{
    bool is_long = arg_len > 1 && arg[0] == '-' && arg[1] == '-';
    size_t long_len;

    for (size_t i = 0; i < options_len; ++i)
    {
        char* token;
        option_t* opt = &options[i];

        switch (opt->type)
        {
            case OPT_BOOL:
                if (!strcmp(is_long ? opt->long_v : opt->short_v, arg))
                {
                    opt->action(user_data);
                }
                break;
                return true;
            case OPT_VALUE:
                if (is_long && (long_len = strlen(opt->long_v)))
                {
                    if (strncmp(opt->long_v, arg, long_len))
                    {
                        break;
                    }
                    if (arg_len < long_len + 2)
                    {
                        printf("%s requires a value\n", opt->long_v);
                        exit(EXIT_FAILURE);
                    }
                    token = arg + strcspn(arg, "=") + 1;
                    opt->action(user_data, token);
                    return true;
                }
                else if (!strcmp(opt->short_v, arg))
                {
                    *waiting_option = opt;
                    return true;
                }
            default: break;
        }
    }

    return false;
}

void args_parse(int argc, char* argv[], option_t* options, size_t options_len, void* user_data)
{
    option_t* waiting_option = NULL;

    const char* app_name = argv[0];

    for (int i = 1; i < argc; ++i)
    {
        char* arg = argv[i];
        size_t arg_len = strlen(arg);

        if (UNLIKELY(arg_len == 0))
        {
            printf("Empty arg%u\n", i);
            exit(EXIT_FAILURE);
        }

        if (waiting_option)
        {
            waiting_option->action(user_data, arg);
            waiting_option = NULL;
            continue;
        }

        if (UNLIKELY(!arg_parse(arg, arg_len, options, options_len, user_data, &waiting_option)))
        {
            if (!strcmp("-h", arg) || !strcmp("--help", arg))
            {
                help_print(app_name, options, options_len);
                exit(EXIT_SUCCESS);
            }

            printf("Unrecognized option: %s\n", arg);
            exit(EXIT_FAILURE);
        }
    }

    if (waiting_option)
    {
        printf("%s requires a value\n", waiting_option->short_v);
        exit(EXIT_FAILURE);
    }
}
