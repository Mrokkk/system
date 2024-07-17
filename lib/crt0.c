#include <stdlib.h>

typedef void (*ctor_t)();

extern ctor_t __init_array_start[], __init_array_end[];
extern ctor_t __fini_array_start[], __fini_array_end[];
extern ctor_t __preinit_array_start[], __preinit_array_end[];

static void _call_global_ctors(void)
{
    for (ctor_t* ctor = __preinit_array_start; ctor != __preinit_array_end; (*ctor++)());
    for (ctor_t* ctor = __init_array_start; ctor != __init_array_end; (*ctor++)());
}

static void _call_global_dtors(void)
{
    for (ctor_t* dtor = __fini_array_start; dtor != __fini_array_end; (*dtor++)());
}

extern int main(int argc, char* const argv[], char* const envp[]);
extern void __libc_start_main(int argc, char* const argv[], char* const envp[]);

__attribute__((noreturn)) void _entry(int argc, char** argv, char** envp)
{
    __libc_start_main(argc, argv, envp);

    _call_global_ctors();

    int ret = main(argc, argv, envp);

    _call_global_dtors();

    while (1)
    {
        exit(ret);
    }
}

__attribute__((noreturn,naked,used)) void _start(int, int, char**, char**)
{
    asm volatile(
        "push $0;"
        "jmp _entry;");
}
