#pragma once

#include <stdbool.h>

#define KERNEL_PARAM(x)      (param_name_t){x}
#define CMDLINE_SIZE         128
#define INIT_IN_PROGRESS     0xfacade
#define CMDLINE_PARAMS_COUNT 32

struct param_name
{
    const char* name;
};

struct param
{
    const char* name;
    const char* value;
};

typedef struct param param_t;
typedef struct param_name param_name_t;

param_t* param_get(param_name_t name);
const char* param_value_get(param_name_t name);
const char* param_value_or_get(param_name_t name, const char* def);
bool param_bool_get(param_name_t name);
void param_call_if_set(param_name_t name, int (*action)());

void arch_setup(void);
void arch_late_setup(void);
int paging_init(void);

extern unsigned init_in_progress;
extern char cmdline[];
extern char* bootloader_name;
