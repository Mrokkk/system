#pragma once

#include <stddef.h>

typedef enum type type_t;
typedef struct syscall syscall_t;

enum type
{
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_LONG,
    TYPE_UNSIGNED_CHAR,
    TYPE_UNSIGNED_SHORT,
    TYPE_UNSIGNED_LONG,
    TYPE_VOID_PTR,
    TYPE_CHAR_PTR,
    TYPE_CONST_CHAR_PTR,
    TYPE_UNRECOGNIZED,
};

struct syscall
{
    const char*     name;
    type_t          ret;
    const size_t    nargs;
    type_t          args[6];
};
