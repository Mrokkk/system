#pragma once

#include <time.h>
#include <getopt.h>
#include <kernel/api/unistd.h>

// https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/basedefs/unistd.h.html

#define _POSIX_VERSION 200809L

#ifndef __ASSEMBLER__

int syscall(int nr, ...);
int isatty(int fd);

static inline int execvp(const char* path, char* const argv[])
{
    extern char** environ;
    return execve(path, argv, environ);
}

unsigned int sleep(unsigned int seconds);

int gethostname(char* name, size_t namelen);
int getpagesize(void);

[[noreturn]] void _exit(int retcode);

extern char** environ;

#endif // __ASSEMBLER__
