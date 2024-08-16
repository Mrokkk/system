#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define DEFAULT_PATH "/bin:/usr/bin"

int LIBC(execv)(const char* pathname, char* const argv[])
{
    return execve(pathname, argv, environ);
}

static void shell_run(const char* file, char* const argv[], char* const envp[])
{
    int argc;
    for (argc = 0; argv[argc]; ++argc)
    {
        if (UNLIKELY(argc == INT_MAX - 2))
        {
            errno = E2BIG;
            return;
        }
    }

    const char* new_argv[argc + 2];
    new_argv[0] = "/bin/bash";
    new_argv[1] = file;

    memcpy(new_argv + 2, argv + 1, argc * sizeof(char*));

    execve(new_argv[0], (char* const*)new_argv, envp);
}

int LIBC(execvpe)(const char* file, char* const argv[], char* const envp[])
{
    if (strchr(file, '/'))
    {
        execve(file, argv, envp);

        if (LIKELY(errno == ENOEXEC))
        {
            shell_run(file, argv, envp);
            return -1;
        }
    }

    const char* path = getenv("PATH");
    const char* delim;
    char* it;

    if (UNLIKELY(!path))
    {
        path = DEFAULT_PATH;
    }

    size_t file_len = strlen(file) + 1;
    size_t path_len = strlen(path) + 1;

    if (UNLIKELY(file_len > NAME_MAX || path_len > PATH_MAX))
    {
        return ERRNO_SET(ENAMETOOLONG, -1);
    }

    char pathname[file_len + path_len];
    bool got_eaccess = false;

    for (const char* p = path;; p = delim)
    {
        delim = strchr(p, ':');

        if (!delim)
        {
            delim = path + strlen(path);
        }

        it = mempcpy(pathname, path, delim - path);
        *it++ = '/';
        memcpy(it, file, file_len);

        execve(pathname, argv, envp);

        if (errno == ENOEXEC)
        {
            shell_run(file, argv, envp);
        }

        switch (errno)
        {
            case EACCES:
                got_eaccess = true;
            case ENOENT:
            case ENOTDIR:
            case ENODEV:
                break;
            default:
                return -1;
        }
    }

    if (got_eaccess)
    {
        errno = EACCES;
    }

    return -1;
}

int LIBC(execvp)(const char* file, char* const argv[])
{
    return execvpe(file, argv, environ);
}

int LIBC(execlp)(const char* file, const char* arg, ...)
{
    int argc;
    va_list args;

    va_start(args, arg);
    for (argc = 1; va_arg(args, const char*); ++argc)
    {
        if (UNLIKELY(argc == INT_MAX))
        {
            va_end(args);
            errno = E2BIG;
            return -1;
        }
    }
    va_end(args);

    const char* argv[argc + 1];
    argv[0] = arg;

    va_start(args, arg);
    for (int i = 1; i <= argc; ++i)
    {
        argv[i] = va_arg(args, const char*);
    }
    va_end(args);

    return execvpe(file, (char* const*)argv, environ);
}

LIBC_ALIAS(execv);
LIBC_ALIAS(execvp);
LIBC_ALIAS(execvpe);
LIBC_ALIAS(execlp);
