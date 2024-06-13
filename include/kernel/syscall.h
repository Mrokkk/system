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
#define __NR_fcntl          14
#define __NR_fork           15
#define __NR_fstat          16
#define __NR_fstatvfs       17
#define __NR_getcwd         18
#define __NR_getdents       19
#define __NR_getegid        20
#define __NR_geteuid        21
#define __NR_getgid         22
#define __NR_getpid         23
#define __NR_getppid        24
#define __NR_getuid         25
#define __NR_init_module    26
#define __NR_ioctl          27
#define __NR_kill           28
#define __NR_lseek          29
#define __NR_mkdir          30
#define __NR_mknod          31
#define __NR_mmap           32
#define __NR_mount          33
#define __NR_open           34
#define __NR_pipe           35
#define __NR_poll           36
#define __NR_read           37
#define __NR_reboot         38
#define __NR_rename         39
#define __NR_sbrk           40
#define __NR_select         41
#define __NR_setgid         42
#define __NR_setsid         43
#define __NR_setuid         44
#define __NR_sigaction      45
#define __NR_signal         46
#define __NR_sigreturn      47
#define __NR_stat           48
#define __NR_statvfs        49
#define __NR_time           50
#define __NR_umask          51
#define __NR_umount         52
#define __NR_umount2        53
#define __NR_unlink         54
#define __NR_waitpid        55
#define __NR_write          56

#define __NR_syscalls       57

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

__syscall2(access, int, const char*, int)
__syscall1(alarm, unsigned int, unsigned int)
__syscall1(chdir, int, const char*)
__syscall1(chroot, int, const char*)
__syscall2(clone, int, unsigned int, void *)
__syscall1(close, int, int)
__syscall2(creat, int, const char*, int)
__syscall1(dtrace, int, int)
__syscall1(dup, int, int)
__syscall2(dup2, int, int, int)
__syscall3(execve, int, const char*, char* const[], char* const[])
__syscall1_noret(exit, void, int)
__syscall2(fchmod, int, int, mode_t)
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
__syscall3(lseek, off_t, int, off_t, int)
__syscall2(mkdir, int, const char*, mode_t)
__syscall3(mknod, int, const char*, mode_t, dev_t)
__syscall6(mmap, void*, void*, size_t, int, int, int, size_t)
__syscall5(mount, int, const char*, const char*, const char*, int, void*)
__syscall3(open, int, const char*, int, int)
__syscall1(pipe, int, int*)
__syscall3(poll, int, struct pollfd*, unsigned long, int)
__syscall3(read, int, int, char*, size_t)
__syscall3(reboot, int, int, int, int)
__syscall2(rename, int, const char*, const char*)
__syscall1(sbrk, void*, size_t)
__syscall5(select, int, int, struct fd_set*, struct fd_set*, struct fd_set*, struct timeval*)
__syscall1(setgid, int, gid_t)
__syscall0(setsid, int)
__syscall1(setuid, int, uid_t)
__syscall3(sigaction, int, int, const struct sigaction*, struct sigaction*)
__syscall2(signal, int, int, int (*)(int))
__syscall1(sigreturn, int, int)
__syscall2(stat, int, const char*, struct stat*)
__syscall2(statvfs, int, const char*, struct statvfs*)
__syscall1(time, time_t, void*)
__syscall1(umask, mode_t, mode_t)
__syscall1(umount, int, const char*)
__syscall2(umount2, int, const char*, int)
__syscall1(unlink, int, const char*)
__syscall3(waitpid, int, int, int*, int)
__syscall3(write, int, int, const char*, size_t)
