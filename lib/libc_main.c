void signals_init(void);
void stdio_init(void);

void __libc_start_main(int, char* const[], char* const envp[])
{
    extern char** environ;
    environ = (char**)envp;
    signals_init();
    stdio_init();
}
