#include <errno.h>
#include <string.h>

void signals_init(void);
void stdio_init(void);

void __libc_start_main(int, char* const argv[], char* const envp[])
{
    char* short_name;
    extern char** environ;
    environ = (char**)envp;
    program_invocation_name = argv[0];
    short_name = strrchr(argv[0], '/');
    program_invocation_short_name = short_name ? short_name + 1 : argv[0];
    signals_init();
    stdio_init();
}
