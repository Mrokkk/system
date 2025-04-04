#pragma once

#include <stdbool.h>
#include <kernel/compiler.h>
#include <kernel/sections.h>

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
void paging_init(void);
void paging_finalize(void);

extern unsigned init_in_progress;
extern char cmdline[];
extern char* bootloader_name;

#define __initcall(fn, type) \
    SECTION(.initcall_##type) int MAYBE_UNUSED((* __init_##fn)(void)) = &fn

#define premodules_initcall(fn) __initcall(fn, premodules)

extern char __initcall_premodules_start[];
extern char __initcall_premodules_end[];
