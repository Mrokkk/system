#pragma once

#include <stddef.h>

typedef void (*action_t)();
typedef struct opt option_t;
typedef enum option_type option_type_t;

enum option_type
{
    OPT_POSITIONAL,
    OPT_VALUE,
    OPT_BOOL
};

struct opt
{
    const char*     short_v;
    const char*     long_v;
    option_type_t   type;
    const char*     description;
    action_t        action;
};

void args_parse(
    int argc,
    char* argv[],
    option_t* options,
    size_t options_len,
    void* user_data);
