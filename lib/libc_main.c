void bss_init(void);
void signals_init(void);

void __libc_start_main(void)
{
    bss_init();
    signals_init();
}
