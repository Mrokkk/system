#pragma once

#include <stdint.h>

#include "list.h"

struct lib
{
    const char* name;
    uintptr_t string_ndx;
    list_head_t list_entry;
};

typedef struct lib lib_t;
