#pragma once

#include <kernel/unistd.h>
#include <time.h>
#include <getopt.h>

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
int pipe(int pipefd[2]);

#endif // __ASSEMBLER__
