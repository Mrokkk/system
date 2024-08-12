#include <stdlib.h>
#include <stdio.h>

extern uintptr_t __stack_chk_guard;
uintptr_t __stack_chk_guard __attribute__((used));

void __attribute__((noreturn)) __stack_chk_fail(void)
{
    fputs("Stack smashing detected!\n", stderr);
    abort();
}

void __attribute__ ((noreturn,visibility("hidden"))) __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}
