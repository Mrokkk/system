#pragma once

typedef struct sys_config sys_config_t;

struct sys_config
{
    int       user_backtrace;
    const int segmexec;
};

int sysfs_init(void);

extern sys_config_t sys_config;
