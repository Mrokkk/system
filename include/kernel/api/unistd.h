#pragma once

#include <common/bits.h>
#include <kernel/api/types.h>

#define SEEK_SET    1
#define SEEK_CUR    2
#define SEEK_END    3

#define R_OK        4   // Test for read permission
#define W_OK        2   // Test for write permission
#define X_OK        1   // Test for execute permission
#define F_OK        0   // Test for existence

#define _POSIX_ARG_MAX      4096
#define ARG_MAX             _POSIX_ARG_MAX

#define _POSIX_OPEN_MAX     32
#define OPEN_MAX            _POSIX_OPEN_MAX

int fork(void);
int getpid(void);
int getppid(void);
__NORETURN void exit(int);
int write(int, const void*, size_t);
int pwrite(int fd, const void* buffer, size_t size, off_t offset);
int read(int, void*, size_t);
int pread(int fd, void* buffer, size_t size, off_t offset);
int open(const char*, int, ...);
int close(int);
int dup(int);
int dup2(int, int);
int waitpid(int, int*, int);
int execve(const char*, char* const argv[], char* const envp[]);
int mount(const char*, const char*, const char*, int, void*);
int umount(const char*);
int umount2(const char* target, int flags);
int mkdir(const char*, mode_t);
int creat(const char*, int);
int getdents(unsigned int, void*, size_t);
int reboot(int magic, int magic2, int cmd);
int chdir(const char* buf);
char* getcwd(char* buf, size_t size);
int brk(void* addr);
void* sbrk(int incr);
int chroot(const char* path);
int setsid(void);
int setuid(gid_t gid);
int setgid(uid_t uid);
int getegid(void);
int getgid(void);
int getuid(void);
int geteuid(void);
off_t lseek(int fildes, off_t offset, int whence);
int unlink(const char* path);
int access(const char* path, int amode);
unsigned int alarm(unsigned int seconds);
int rename(const char* oldpath, const char* newpath);
int pipe(int pipefd[2]);
int lchown(const char* pathname, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int fsync(int fildes);
int rmdir(const char* pathname);

ssize_t readlink(const char* pathname, char* buf, size_t bufsiz);

// FIXME: replace later with ptrace
#define DTRACE_FOLLOW_FORK (1 << 1)
#define DTRACE_BACKTRACE   (1 << 2)
int dtrace(int flag);
