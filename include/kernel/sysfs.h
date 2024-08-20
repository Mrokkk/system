#pragma once

typedef struct sys_config sys_config_t;

struct sys_config
{
    int user_backtrace;
    int trace_backtrace;
};

int sysfs_init(void);

extern sys_config_t sys_config;
