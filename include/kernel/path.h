#pragma once

#include <kernel/dentry.h>
#include <kernel/string.h>

static inline int path_is_absolute(const char* path)
{
    return path[0] == '/';
}

static inline char* path_next(char* path)
{
    return strchr(path, '/');
}

static inline const char* find_last_slash(const char* path)
{
    const char* last_slash = strrchr(path, '/');
    return last_slash
        ? last_slash
        : path;
}

int construct_path(dentry_t* dentry, char* output, size_t size);
int path_validate(const char* path);
void dirname(const char* path, char* output);
