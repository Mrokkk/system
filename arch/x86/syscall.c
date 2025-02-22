#include <arch/cpuid.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/page_types.h>

typedef int (*syscall_t)();

#define __syscall(call, ...)    int sys_##call();
#define __syscall0(...)         __syscall(__VA_ARGS__)
#define __syscall1(...)         __syscall(__VA_ARGS__)
#define __syscall1_noret(...)   __syscall(__VA_ARGS__)
#define __syscall2(...)         __syscall(__VA_ARGS__)
#define __syscall3(...)         __syscall(__VA_ARGS__)
#define __syscall4(...)         __syscall(__VA_ARGS__)
#define __syscall5(...)         __syscall(__VA_ARGS__)
#define __syscall6(...)         __syscall(__VA_ARGS__)
#include <kernel/api/syscall.h>

#undef __syscall
#undef __syscall0
#undef __syscall1
#undef __syscall1_noret
#undef __syscall2
#undef __syscall3
#undef __syscall4
#undef __syscall5
#undef __syscall6
#define __syscall(call, ...)    [__NR_##call] = &sys_##call,
#define __syscall0(...)         __syscall(__VA_ARGS__)
#define __syscall1(...)         __syscall(__VA_ARGS__)
#define __syscall1_noret(...)   __syscall(__VA_ARGS__)
#define __syscall2(...)         __syscall(__VA_ARGS__)
#define __syscall3(...)         __syscall(__VA_ARGS__)
#define __syscall4(...)         __syscall(__VA_ARGS__)
#define __syscall5(...)         __syscall(__VA_ARGS__)
#define __syscall6(...)         __syscall(__VA_ARGS__)

READONLY const syscall_t syscalls[] = {
    #include <kernel/api/syscall.h>
};

#define IA32_SYSENTER_CS  0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

extern void vsyscall_handler();

UNMAP_AFTER_INIT int vsyscall_init(void)
{
    page_t* page;

    if (!cpu_has(X86_FEATURE_SEP))
    {
        return 0;
    }

    page = page_alloc(1, 0);

    if (!page)
    {
        return -1;
    }

    wrmsr(IA32_SYSENTER_ESP, page_virt(page) + PAGE_SIZE, 0);
    wrmsr(IA32_SYSENTER_CS, KERNEL_CS, 0);
    wrmsr(IA32_SYSENTER_EIP, &vsyscall_handler, 0);

    return 0;
}

int do_vsyscall(int nr, pt_regs_t)
{
    switch (nr)
    {
        case __NR_getpid:
            return process_current->pid;
        case __NR_getppid:
            return process_current->parent->pid;
        default:
            return -ENOSYS;
    }
}
