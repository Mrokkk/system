#ifndef __SYSCALL_H
#define __SYSCALL_H

#define __NR_access         1
#define __NR_alarm          2
#define __NR_chdir          3
#define __NR_chroot         4
#define __NR_clone          5
#define __NR_close          6
#define __NR_creat          7
#define __NR_dtrace         8
#define __NR_dup            9
#define __NR_dup2           10
#define __NR_execve         11
#define __NR_exit           12
#define __NR_fchmod         13
#define __NR_fchown         14
#define __NR_fcntl          15
#define __NR_fork           16
#define __NR_fstat          17
#define __NR_fstatvfs       18
#define __NR_getcwd         19
#define __NR_getdents       20
#define __NR_getegid        21
#define __NR_geteuid        22
#define __NR_getgid         23
#define __NR_getpid         24
#define __NR_getppid        25
#define __NR_getuid         26
#define __NR_init_module    27
#define __NR_ioctl          28
#define __NR_kill           29
#define __NR_lchown         30
#define __NR_lseek          31
#define __NR_mkdir          32
#define __NR_mknod          33
#define __NR_mmap           34
#define __NR_mount          35
#define __NR_open           36
#define __NR_pipe           37
#define __NR_poll           38
#define __NR_pselect        39
#define __NR_read           40
#define __NR_reboot         41
#define __NR_rename         42
#define __NR_sbrk           43
#define __NR_select         44
#define __NR_setgid         45
#define __NR_setsid         46
#define __NR_setuid         47
#define __NR_sigaction      48
#define __NR_signal         49
#define __NR_sigreturn      50
#define __NR_stat           51
#define __NR_statvfs        52
#define __NR_time           53
#define __NR_umask          54
#define __NR_umount         55
#define __NR_umount2        56
#define __NR_unlink         57
#define __NR_waitpid        58
#define __NR_write          59
#define __NR_timer_create   60
#define __NR_timer_settime  61
#define __NR_statfs         62
#define __NR_fstatfs        63
#define __NR_brk            64
#define __NR_mprotect       65
#define __NR_rmdir          66
#define __NR_pread          67
#define __NR_pwrite         68
#define __NR__syslog        69
#define __NR_gettimeofday   70
#define __NR_clock_gettime  71
#define __NR_clock_settime  72
#define __NR_getpgrp        73
#define __NR_setpgrp        74
#define __NR_readlink       75
#define __NR_fchdir         76
#define __NR_lstat          77
#define __NR_mimmutable     78
#define __NR_munmap         79
#define __NR_set_thread_area 80
#define __NR_pinsyscalls    81
#define __NR_chmod          82
#define __NR_timer_delete   83

#define __NR_syscalls       84

#ifndef __ASSEMBLER__

struct fd_set;
struct itimerspec;
struct pollfd;
struct sigaction;
struct sigevent;
struct stat;
struct statfs;
struct statvfs;
struct timespec;
struct timeval;
struct timezone;

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
#ifndef __syscall4
#define __syscall4(...)
#endif
#ifndef __syscall5
#define __syscall5(...)
#endif
#ifndef __syscall6
#define __syscall6(...)
#endif

__syscall2(access, int, const char*, int)
__syscall1(alarm, unsigned int, unsigned int)
__syscall1(chdir, int, const char*)
__syscall1(chroot, int, const char*)
__syscall5(clone, int, int (*)(void*), void*, int, void*, void*)
__syscall1(close, int, int)
__syscall2(creat, int, const char*, int)
__syscall1(dtrace, int, int)
__syscall1(dup, int, int)
__syscall2(dup2, int, int, int)
__syscall3(execve, int, const char*, char* const[], char* const[])
__syscall1_noret(exit, void, int)
__syscall2(fchmod, int, int, mode_t)
__syscall3(fchown, int, int, uid_t, gid_t)
__syscall3(fcntl, int, int, int, int)
__syscall0(fork, int)
__syscall2(fstat, int, int, struct stat*)
__syscall2(fstatvfs, int, int, struct statvfs*)
__syscall2(getcwd, int, char*, size_t)
__syscall3(getdents, int, unsigned int, void*, size_t)
__syscall0(getegid, int)
__syscall0(geteuid, int)
__syscall0(getgid, int)
__syscall0(getpid, int)
__syscall0(getppid, int)
__syscall0(getuid, int)
__syscall1(init_module, int, const char*)
__syscall3(ioctl, int, int, unsigned long, unsigned long)
__syscall2(kill, int, int, int)
__syscall3(lchown, int, const char*, uid_t, gid_t)
__syscall3(lseek, off_t, int, off_t, int)
__syscall2(mkdir, int, const char*, mode_t)
__syscall3(mknod, int, const char*, mode_t, dev_t)
__syscall6(mmap, void*, void*, size_t, int, int, int, size_t)
__syscall5(mount, int, const char*, const char*, const char*, int, void*)
__syscall3(open, int, const char*, int, int)
__syscall1(pipe, int, int*)
__syscall3(poll, int, struct pollfd*, unsigned long, int)
__syscall6(pselect, int, int, fd_set*, fd_set*, fd_set*, const struct timespec*, const sigset_t*)
__syscall3(read, int, int, char*, size_t)
__syscall3(reboot, int, int, int, int)
__syscall2(rename, int, const char*, const char*)
__syscall1(sbrk, void*, size_t)
__syscall5(select, int, int, struct fd_set*, struct fd_set*, struct fd_set*, struct timeval*)
__syscall1(setgid, int, gid_t)
__syscall0(setsid, int)
__syscall1(setuid, int, uid_t)
__syscall3(sigaction, int, int, const struct sigaction*, struct sigaction*)
__syscall2(signal, int, int, void (*)(int))
__syscall1(sigreturn, int, int)
__syscall2(stat, int, const char*, struct stat*)
__syscall2(statvfs, int, const char*, struct statvfs*)
__syscall1(time, time_t, time_t*)
__syscall1(umask, mode_t, mode_t)
__syscall1(umount, int, const char*)
__syscall2(umount2, int, const char*, int)
__syscall1(unlink, int, const char*)
__syscall3(waitpid, int, int, int*, int)
__syscall3(write, int, int, const char*, size_t)
__syscall3(timer_create, int, clockid_t, struct sigevent*, timer_t*)
__syscall4(timer_settime, int, timer_t, int, const struct itimerspec*, struct itimerspec*)
__syscall2(statfs, int, const char*, struct statfs*)
__syscall2(fstatfs, int, int, struct statfs*)
__syscall1(brk, int, void*)
__syscall3(mprotect, int, void*, size_t, int)
__syscall1(rmdir, int, const char*)
__syscall4(pread, int, int, void*, size_t, off_t)
__syscall3(pwrite, int, const void*, size_t, off_t)
__syscall3(_syslog, int, int, int, const char*)
__syscall2(gettimeofday, int, struct timeval*, struct timezone*)
__syscall2(clock_gettime, int, clockid_t, struct timespec*)
__syscall2(clock_settime, int, clockid_t, const struct timespec*)
__syscall0(getpgrp, pid_t)
__syscall0(setpgrp, pid_t)
__syscall3(readlink, int, const char*, char*, size_t)
__syscall1(fchdir, int, int)
__syscall2(lstat, int, const char*, struct stat*)
__syscall2(mimmutable, int, void*, size_t)
__syscall2(munmap, int, void*, size_t)
__syscall1(set_thread_area, int, uintptr_t)
__syscall2(pinsyscalls, int, void*, size_t)
__syscall2(chmod, int, const char*, mode_t)
__syscall1(timer_delete, int, timer_t)
