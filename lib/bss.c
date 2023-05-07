void bss_init()
{
    extern char _sbss[], _ebss[];
    __builtin_memset(_sbss, 0, _ebss - _sbss);
}
