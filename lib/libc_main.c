#include <errno.h>
#include <stdlib.h>
#include <string.h>

void signals_init(void);
void stdio_init(void);

int libc_debug;

void __libc_start_main(int, char* const argv[], char* const envp[])
{
    char* short_name;
    extern char** environ;
    environ = (char**)envp;
    program_invocation_name = argv[0];
    short_name = strrchr(argv[0], '/');
    program_invocation_short_name = short_name ? short_name + 1 : argv[0];

    if (getenv("LIBC_DEBUG"))
    {
        libc_debug = 1;
    }

    signals_init();
    stdio_init();
}

#define STUB(name) \
    void name() {}

STUB(__register_frame_info);
STUB(__deregister_frame_info);
STUB(_ITM_registerTMCloneTable);
STUB(_ITM_deregisterTMCloneTable);
STUB(__cxa_finalize); // FIXME: this shouldn't be a stub
