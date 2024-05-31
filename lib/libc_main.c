void signals_init(void);

void __libc_start_main(void)
{
    signals_init();
}
