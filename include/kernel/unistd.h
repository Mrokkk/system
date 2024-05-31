#pragma once

#include <kernel/types.h>

int fork(void);
int getpid(void);
int getppid(void);
[[noreturn]] void exit(int);
int write(int, const char*, size_t);
int read(int, char*, size_t);
int open(const char*, int, ...);
int close(int);
int dup(int);
int dup2(int, int);
int waitpid(int, int*, int);
int exec(const char*, char* const argv[]);
int mount(const char*, const char*, const char*, int, void*);
int umount(const char*);
int umount2(const char* target, int flags);
int mkdir(const char*, mode_t);
int creat(const char*, int);
int clone(unsigned int flags, void* stack);
int getdents(unsigned int, void*, size_t);
int reboot(int magic, int magic2, int cmd);
int chdir(const char* buf);
char* getcwd(char* buf, size_t size);
void* sbrk(size_t incr);
int chroot(const char* path);
int setsid(void);
int setuid(gid_t gid);
int setgid(uid_t uid);
