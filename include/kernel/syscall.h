#ifndef __SYSCALL_H
#define __SYSCALL_H

#define __NR_chdir          1
#define __NR_chroot         2
#define __NR_clone          3
#define __NR_close          4
#define __NR_creat          5
#define __NR_dup            6
#define __NR_dup2           7
#define __NR_exec           8
#define __NR_exit           9
#define __NR_fork           10
#define __NR_fstat          11
#define __NR_fstatvfs       12
#define __NR_getcwd         13
#define __NR_getdents       14
#define __NR_geteuid        15
#define __NR_getgid         16
#define __NR_getpid         17
#define __NR_getppid        18
#define __NR_getuid         19
#define __NR_ioctl          20
#define __NR_kill           21
#define __NR_lseek          22
#define __NR_mkdir          23
#define __NR_mmap           24
#define __NR_mount          25
#define __NR_open           26
#define __NR_poll           27
#define __NR_read           28
#define __NR_reboot         29
#define __NR_sbrk           30
#define __NR_select         31
#define __NR_setsid         32
#define __NR_sigaction      33
#define __NR_signal         34
#define __NR_sigreturn      35
#define __NR_stat           36
#define __NR_statvfs        37
#define __NR_time           38
#define __NR_umount         39
#define __NR_umount2        40
#define __NR_waitpid        41
#define __NR_write          42

#define __NR_syscalls       43

#ifndef __ASSEMBLER__

struct fd_set;
struct pollfd;
struct sigaction;
struct stat;
struct statvfs;
struct timeval;

#endif  // __ASSEMBLER__

#endif  // __SYSCALL_H

#ifndef __syscall0
#define __syscall0(...)
#endif
#ifndef __syscall1
#define __syscall1(...)
#endif
#ifndef __syscall1_noret
#define __syscall1_noret(...)
#endif
#ifndef __syscall2
#define __syscall2(...)
#endif
#ifndef __syscall3
#define __syscall3(...)
#endif
#ifndef __syscall5
#define __syscall5(...)
#endif
#ifndef __syscall6
#define __syscall6(...)
#endif

__syscall1(chdir, int, const char*)
__syscall1(chroot, int, const char*)
__syscall2(clone, int, unsigned int, void *)
__syscall1(close, int, int)
__syscall2(creat, int, const char*, int)
__syscall1(dup, int, int)
__syscall2(dup2, int, int, int)
__syscall2(exec, int, const char*, char* const[])
__syscall1_noret(exit, void, int)
__syscall0(fork, int)
__syscall2(fstat, int, int, struct stat*)
__syscall2(fstatvfs, int, int, struct statvfs*)
__syscall2(getcwd, int, char*, size_t)
__syscall3(getdents, int, unsigned int, void*, size_t)
__syscall0(geteuid, int)
__syscall0(getgid, int)
__syscall0(getpid, int)
__syscall0(getppid, int)
__syscall0(getuid, int)
__syscall3(ioctl, int, int, unsigned long, unsigned long)
__syscall2(kill, int, int, int)
__syscall3(lseek, off_t, int, off_t, int)
__syscall2(mkdir, int, const char*, mode_t)
__syscall6(mmap, void*, void*, size_t, int, int, int , size_t)
__syscall5(mount, int, const char*, const char*, const char*, int, void*)
__syscall3(open, int, const char*, int, int)
__syscall3(poll, int, struct pollfd*, unsigned long, int)
__syscall3(read, int, int, char*, size_t)
__syscall3(reboot, int, int, int, int)
__syscall1(sbrk, void*, size_t)
__syscall5(select, int, int, struct fd_set*, struct fd_set*, struct fd_set*, struct timeval*)
__syscall0(setsid, int)
__syscall3(sigaction, int, int, const struct sigaction*, struct sigaction*)
__syscall2(signal, int, int, int (*)(int))
__syscall1(sigreturn, int, int)
__syscall2(stat, int, const char*, struct stat*)
__syscall2(statvfs, int, const char*, struct statvfs*)
__syscall1(time, time_t, void*)
__syscall1(umount, int, const char*)
__syscall2(umount2, int, const char*, int)
__syscall3(waitpid, int, int, int*, int)
__syscall3(write, int, int, const char*, size_t)
