__attribute__((naked,noreturn,used)) void _start(int, char**)
{
    asm volatile(
        "call __libc_start_main;"
        "call main;"
        "push %eax;"
        "call exit;");
}
