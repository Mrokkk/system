#include "libc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** environ;
size_t environ_size;

typedef char* env_t;
typedef struct env var_t;

struct env
{
    int id;
    const char* name;
    size_t name_len;
    char* value;
};

static int var_find(const char* name, var_t* env)
{
    size_t name_len;
    size_t len = strlen(name);
    for (int i = 0; environ[i]; ++i)
    {
        name_len = strcspn(environ[i], "=");
        if (name_len == len && !strncmp(name, environ[i], name_len))
        {
            env->id = i;
            env->name = environ[i];
            env->value = environ[i] + name_len + 1;
            env->name_len = name_len;
            return 0;
        }
    }
    return -1;
}

static int var_find_empty(char*** empty)
{
    size_t i;
    for (i = 0; environ[i]; ++i);
    if (i < environ_size)
    {
        *empty = environ + i;
        return 0;
    }
    return -1;
}

char* LIBC(getenv)(const char* name)
{
    var_t var;

    if (var_find(name, &var))
    {
        return NULL;
    }

    return var.value;
}
WEAK_ALIAS(LIBC(getenv), getenv)

char* LIBC(secure_getenv)(const char* name)
{
    return getenv(name);
}
WEAK_ALIAS(LIBC(secure_getenv), secure_getenv)

static int var_allocate(const char* name, const char* value, size_t name_len, size_t value_len, char** var)
{
    char* new_var = SAFE_ALLOC(malloc(name_len + value_len + 2), -1);
    sprintf(new_var, "%s=%s", name, value);
    *var = new_var;
    return 0;
}

static int var_reallocate(var_t* var, const char* value, size_t value_len)
{
    char* new_var = SAFE_ALLOC(malloc(value_len + var->name_len + 2), -1);

    strncpy(new_var, var->name, var->name_len);
    sprintf(new_var + var->name_len, "=%s", value);

    if (environ_size)
    {
        free(environ[var->id]);
    }

    environ[var->id] = new_var;

    return 0;
}

static int env_reallocate(char*** empty_env)
{
    size_t new_size = environ_size ? environ_size * 2 : 32;
    char** new_environ = SAFE_ALLOC(malloc((new_size + 1) * sizeof(char*)), -1);
    int i;

    for (i = 0; environ[i]; ++i)
    {
        new_environ[i] = environ[i];
    }

    memset(new_environ + i, 0, (new_size - i) * sizeof(char*));

    *empty_env = new_environ + i;

    if (environ_size)
    {
        free(environ);
    }

    environ_size = new_size;
    environ = new_environ;

    return 0;
}

int LIBC(setenv)(const char* name, const char* value, int overwrite)
{
    VALIDATE_INPUT(name, -1);

    var_t old_var;
    size_t new_value_len = strlen(value);

    if (!var_find(name, &old_var))
    {
        size_t old_value_len = strlen(old_var.value);

        if (!overwrite)
        {
            return 0;
        }

        if (new_value_len <= old_value_len)
        {
            strcpy(old_var.value, value);
        }
        else
        {
            return var_reallocate(&old_var, value, new_value_len);
        }
    }
    else
    {
        char** empty = NULL;
        size_t name_len = strlen(name);
        if (!environ_size)
        {
            if (UNLIKELY(env_reallocate(&empty)))
            {
                return -1;
            }
        }
        else
        {
            if (var_find_empty(&empty))
            {
                if (UNLIKELY(env_reallocate(&empty)))
                {
                    return -1;
                }
            }
        }
        return var_allocate(name, value, name_len, new_value_len, empty);
    }

    return 0;
}
WEAK_ALIAS(LIBC(setenv), setenv)
