#ifndef __SYSCALLS_H_
#define __SYSCALLS_H_

#define __NR_exit 1
#define __NR_fork 2
#define __NR_read 3
#define __NR_write 4
#define __NR_open 5
#define __NR_close 6
#define __NR_waitpid 7
#define __NR_creat 8
#define __NR_link 9
#define __NR_unlink 10
#define __NR_exec 11
#define __NR_chdir 12
#define __NR_time 13
#define __NR_mknod 14
#define __NR_chmod 15
#define __NR_lchown 16
#define __NR_break 17
#define __NR_stat 18
#define __NR_lseek 19
#define __NR_getpid 20
#define __NR_mount 21
#define __NR_umount 22
#define __NR_setuid 23
#define __NR_getuid 24
#define __NR_stime 25
#define __NR_ptrace 26
#define __NR_alarm 27
#define __NR_fstat 28
#define __NR_pause 29
#define __NR_utime 30
#define __NR_stty 31
#define __NR_gtty 32
#define __NR_mkdir 33
#define __NR_nice 34
#define __NR_sleep 35
#define __NR_sync 36
#define __NR_kill 37
#define __NR_switch 38
#define __NR_reboot 39
#define __NR_getcwd 40
#define __NR_dup 41
#define __NR_pipe 42
#define __NR_times 43
#define __NR_prof 44
#define __NR_getdents 45
#define __NR_setgid 46
#define __NR_getgid 47
#define __NR_signal 48
#define __NR_readdir 49
#define __NR_dup2 50
#define __NR_sbrk 51
#define __NR_getppid 64
#define __NR_sigreturn 119
#define __NR_clone 120

#define __NR_syscalls 120

#ifndef __ASSEMBLER__

#include <kernel/types.h>

int fork(void);
pid_t getpid(void);
pid_t getppid(void);
void exit(int);
int write(int, const char*, size_t);
int read(int, void*, size_t);
int open(const char*, int, int);
int close(int);
int dup(int);
int waitpid(int, int*, int);
int exec(const char*, char* const argv[]);
int mount(const char*, const char*, const char*, int, void*);
int mkdir(const char*, int);
int creat(const char*, int);
int clone(unsigned int flags, void* stack);
int getdents(unsigned int, void*, size_t);
int reboot(int magic, int magic2, int cmd);
int chdir(const char* buf);
int getcwd(char* buf, size_t size);
void* sbrk(size_t incr);

#endif // __ASSEMBLER__

#endif // __SYSCALLS_H_

#ifndef __syscall0
#define __syscall0(call)
#endif

#ifndef __syscall1
#define __syscall1(call, type1)
#endif

#ifndef __syscall2
#define __syscall2(call, type1, type2)
#endif

#ifndef __syscall3
#define __syscall3(call, type1, type2, type3)
#endif

#ifndef __syscall4
#define __syscall4(call, type1, type2, type3, type4)
#endif

#ifndef __syscall5
#define __syscall5(call, type1, type2, type3, type4, type5)
#endif

#define TYPE_UL_HEX        1
#define TYPE_UL_DEC        2
#define TYPE_SL_DEC        3
#define TYPE_UW_HEX        4
#define TYPE_UW_DEC        5
#define TYPE_UB_HEX        6
#define TYPE_UB_DEC        7
#define TYPE_STRING        8
#define TYPE_SIGNAL        9

__syscall1(exit, TYPE_SL_DEC)
__syscall0(fork)
__syscall3(read, TYPE_UL_DEC, TYPE_STRING, TYPE_UL_DEC)
__syscall3(write, TYPE_UL_DEC, TYPE_STRING, TYPE_UL_DEC)
__syscall3(open, TYPE_STRING, TYPE_UL_DEC, TYPE_UL_DEC)
__syscall1(dup, TYPE_SL_DEC)
__syscall1(close, int)
__syscall3(waitpid, int, int *, int)
__syscall2(exec, const char*, char* const[])
__syscall5(mount, const char *, const char *, const char *, 0, 0)
__syscall2(clone, unsigned int, void *)
__syscall2(kill, int, int)
__syscall2(signal, int, int (*)(int))
__syscall1(sigreturn, int)
__syscall0(getpid)
__syscall0(getppid)
__syscall2(mkdir, const char*, int)
__syscall2(creat, const char*, int)
__syscall3(getdents, unsigned int, void*, size_t)
__syscall3(reboot, int, int, int)
__syscall1(chdir, const char*)
__syscall2(getcwd, char*, size_t)
__syscall2(dup2, int, int)
__syscall1(sbrk, size_t)
__syscall2(stat, const char*, struct stat*)
