#include <kernel/trace.h>
#include <kernel/kernel.h>
#include <kernel/api/types.h>

syscall_t trace_syscalls[] = {
    { },

    {
        .name   = "access",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_LONG },
    },

    {
        .name   = "alarm",
        .ret    = TYPE_UNSIGNED_LONG,
        .nargs  = 1,
        .args   = { TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "chdir",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_CONST_CHAR_PTR },
    },

    {
        .name   = "chroot",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_CONST_CHAR_PTR },
    },

    {
        .name   = "clone",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_UNSIGNED_LONG, TYPE_VOID_PTR },
    },

    {
        .name   = "close",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_LONG },
    },

    {
        .name   = "creat",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_LONG },
    },

    {
        .name   = "dtrace",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_LONG },
    },

    {
        .name   = "dup",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_LONG },
    },

    {
        .name   = "dup2",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_LONG, TYPE_LONG },
    },

    {
        .name   = "execve",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_VOID_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "exit",
        .ret    = TYPE_VOID,
        .nargs  = 1,
        .args   = { TYPE_LONG },
    },

    {
        .name   = "fchmod",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_LONG, TYPE_UNSIGNED_SHORT },
    },

    {
        .name   = "fchown",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_UNSIGNED_SHORT, TYPE_LONG },
    },

    {
        .name   = "fcntl",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_LONG, TYPE_LONG },
    },

    {
        .name   = "fork",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "fstat",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_LONG, TYPE_VOID_PTR },
    },

    {
        .name   = "fstatvfs",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_LONG, TYPE_VOID_PTR },
    },

    {
        .name   = "getcwd",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CHAR_PTR, TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "getdents",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_UNSIGNED_LONG, TYPE_VOID_PTR, TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "getegid",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "geteuid",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "getgid",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "getpid",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "getppid",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "getuid",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "init_module",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_CONST_CHAR_PTR },
    },

    {
        .name   = "ioctl",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_UNSIGNED_LONG, TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "kill",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_LONG, TYPE_LONG },
    },

    {
        .name   = "lchown",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_UNSIGNED_SHORT, TYPE_LONG },
    },

    {
        .name   = "lseek",
        .ret    = TYPE_UNSIGNED_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_UNSIGNED_LONG, TYPE_LONG },
    },

    {
        .name   = "mkdir",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_UNSIGNED_SHORT },
    },

    {
        .name   = "mknod",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_UNSIGNED_SHORT, TYPE_UNSIGNED_SHORT },
    },

    {
        .name   = "mmap",
        .ret    = TYPE_VOID_PTR,
        .nargs  = 6,
        .args   = { TYPE_VOID_PTR, TYPE_UNSIGNED_LONG, TYPE_LONG, TYPE_LONG, TYPE_LONG, TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "mount",
        .ret    = TYPE_LONG,
        .nargs  = 5,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_CONST_CHAR_PTR, TYPE_CONST_CHAR_PTR, TYPE_LONG, TYPE_VOID_PTR },
    },

    {
        .name   = "open",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_LONG, TYPE_LONG },
    },

    {
        .name   = "pipe",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_VOID_PTR },
    },

    {
        .name   = "poll",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_VOID_PTR, TYPE_UNSIGNED_LONG, TYPE_LONG },
    },

    {
        .name   = "pselect",
        .ret    = TYPE_LONG,
        .nargs  = 6,
        .args   = { TYPE_LONG, TYPE_VOID_PTR, TYPE_VOID_PTR, TYPE_VOID_PTR, TYPE_VOID_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "read",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_CHAR_PTR, TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "reboot",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_LONG, TYPE_LONG },
    },

    {
        .name   = "rename",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_CONST_CHAR_PTR },
    },

    {
        .name   = "sbrk",
        .ret    = TYPE_VOID_PTR,
        .nargs  = 1,
        .args   = { TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "select",
        .ret    = TYPE_LONG,
        .nargs  = 5,
        .args   = { TYPE_LONG, TYPE_VOID_PTR, TYPE_VOID_PTR, TYPE_VOID_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "setgid",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_LONG },
    },

    {
        .name   = "setsid",
        .ret    = TYPE_LONG,
        .nargs  = 0,
        .args   = {  },
    },

    {
        .name   = "setuid",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_UNSIGNED_SHORT },
    },

    {
        .name   = "sigaction",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_VOID_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "signal",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_LONG, TYPE_VOID_PTR },
    },

    {
        .name   = "sigreturn",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_LONG },
    },

    {
        .name   = "stat",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "statvfs",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "time",
        .ret    = TYPE_UNSIGNED_LONG,
        .nargs  = 1,
        .args   = { TYPE_VOID_PTR },
    },

    {
        .name   = "umask",
        .ret    = TYPE_UNSIGNED_SHORT,
        .nargs  = 1,
        .args   = { TYPE_UNSIGNED_SHORT },
    },

    {
        .name   = "umount",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_CONST_CHAR_PTR },
    },

    {
        .name   = "umount2",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_LONG },
    },

    {
        .name   = "unlink",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_CONST_CHAR_PTR },
    },

    {
        .name   = "waitpid",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_VOID_PTR, TYPE_LONG },
    },

    {
        .name   = "write",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_LONG, TYPE_CONST_CHAR_PTR, TYPE_UNSIGNED_LONG },
    },

    {
        .name   = "timer_create",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_UNSIGNED_LONG, TYPE_VOID_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "timer_settime",
        .ret    = TYPE_LONG,
        .nargs  = 4,
        .args   = { TYPE_UNSIGNED_LONG, TYPE_LONG, TYPE_VOID_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "statfs",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_CONST_CHAR_PTR, TYPE_VOID_PTR },
    },

    {
        .name   = "fstatfs",
        .ret    = TYPE_LONG,
        .nargs  = 2,
        .args   = { TYPE_LONG, TYPE_VOID_PTR },
    },

    {
        .name   = "brk",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_VOID_PTR },
    },

    {
        .name   = "mprotect",
        .ret    = TYPE_LONG,
        .nargs  = 3,
        .args   = { TYPE_VOID_PTR, TYPE_UNSIGNED_LONG, TYPE_LONG },
    },

    {
        .name   = "rmdir",
        .ret    = TYPE_LONG,
        .nargs  = 1,
        .args   = { TYPE_CONST_CHAR_PTR },
    },
};