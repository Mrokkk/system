#pragma once

struct param
{
    const char* name;
    const char* value;
};

typedef struct param param_t;

void arch_setup(void);
void arch_late_setup(void);
int paging_init(void);
param_t* param_get(const char* name);
const char* param_value_get(const char* name);
const char* param_value_or_get(const char* name, const char* def);
int param_bool_get(const char* name);
void param_call_if_set(const char* name, int (*action)());

#define KERNEL_PARAM(x) x

#define INIT_IN_PROGRESS 0xfacade
extern unsigned init_in_progress;
extern char cmdline[];
