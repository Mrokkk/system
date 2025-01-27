#pragma once

#include <time.h>
#include <bits/getopt.h>
#include <common/bits.h>
#include <kernel/api/unistd.h>

// https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/basedefs/unistd.h.html

#define _POSIX_VERSION 200809L

// Flags for sysconf
#define _SC_ARG_MAX         1
#define _SC_OPEN_MAX        2

// Flags for confstr
#define _CS_PATH            1

#ifndef __ASSEMBLER__

int syscall(int nr, ...);
int isatty(int fd);

int execlp(const char* file, const char* arg, ...) __attribute__((sentinel));

int execv(const char* pathname, char* const argv[]);
int execvp(const char* file, char* const argv[]);
int execvpe(const char* file, char* const argv[], char* const envp[]);

unsigned int sleep(unsigned int seconds);

int gethostname(char* name, size_t namelen);
int getpagesize(void);

__NORETURN void _exit(int retcode);

long sysconf(int name);

int ftruncate(int fildes, off_t length);

size_t confstr(int name, char* buf, size_t size);

int symlink(const char* target, const char* linkpath);

extern char** environ;

#endif // __ASSEMBLER__
