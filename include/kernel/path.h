#pragma once

#include <kernel/dentry.h>
#include <kernel/string.h>

static inline int path_is_absolute(const char* path)
{
    return *path == '/';
}

static inline const char* path_next(const char* path)
{
    return strchr(path, '/');
}

static inline const char* basename_get(const char* path)
{
    const char* last_slash = strrchr(path, '/');
    return last_slash
        ? last_slash + 1
        : path;
}

int path_construct(const dentry_t* dentry, char* output, size_t size);

int path_validate(const char* path);
int dirname_get(const char* path, char* output, size_t size);
