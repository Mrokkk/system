typedef void (*ctor_t)();

extern ctor_t __init_array_start[], __init_array_end[];
extern ctor_t __fini_array_start[], __fini_array_end[];
extern ctor_t __preinit_array_start[], __preinit_array_end[];

__attribute__((used)) void _call_global_ctors(void)
{
    for (ctor_t* ctor = __preinit_array_start; ctor != __preinit_array_end; (*ctor++)());
    for (ctor_t* ctor = __init_array_start; ctor != __init_array_end; (*ctor++)());
}

__attribute__((used)) void _call_global_dtors(void)
{
    for (ctor_t* dtor = __fini_array_start; dtor != __fini_array_end; (*dtor++)());
}

__attribute__((naked,noreturn,used)) void _start(int, int, char**, char**)
{
    asm volatile(
        "call __libc_start_main;"
        "call _call_global_ctors;"
        "call main;"
        "push %eax;"
        "call _call_global_dtors;"
        "call exit;");
}
