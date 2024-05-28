void bss_init()
{
    extern char __bss_start[], _end[];
    __builtin_memset(__bss_start, 0, _end - __bss_start);
}
