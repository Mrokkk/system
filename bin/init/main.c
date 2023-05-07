#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <kernel/reboot.h>

typedef struct options
{
    const char* cmdline;
    char console_device[32];
} options_t;

static inline const char* parse_key_value(
    char* dest,
    const char* src,
    const char* key)
{
    if (!strncmp(key, src, strlen(key)))
    {
        src = strchr(src, '=') + 1;
        const char* next = strchr(src, ' ');
        size_t len = strlen(src);

        char* string = strncpy(
            dest,
            src,
            next ? (size_t)(next - src) : len);

        if (next)
        {
            src = next + 1;
        }
        else
        {
            src += len;
        }
        *string = 0;
    }

    return src;
}

static inline void parse_cmdline(options_t* options, const char* cmdline)
{
    const char* temp = cmdline;

    options->cmdline = cmdline;

    while (*temp)
    {
        temp = parse_key_value(options->console_device, temp, "console=");
        ++temp;
    }
}

int shell_run()
{
    int child_pid, status;

    if ((child_pid = fork()) < 0)
    {
        printf("fork error in init!\n");
        return EXIT_FAILURE;
    }
    else if (child_pid == 0)
    {
        char* pathname = "/bin/sh";
        char* const argv[] = {pathname, NULL, };
        if (exec(pathname, argv))
        {
            printf("exec error in init!\n");
            return EXIT_FAILURE;
        }
        while (1);
    }

    waitpid(child_pid, &status, 0);

    printf("shell exited with %d, performing reboot\n", status);

    reboot(REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_CMD_RESTART);

    while (1);
}

#define RED         "\e[31m"
#define GREEN       "\e[32m"
#define YELLOW      "\e[33m"
#define BLUE        "\e[34m"
#define MAGENTA     "\e[35m"
#define CYAN        "\e[36m"
#define BG          "\e[30;44m"
#define RESET       "\e[0m"

int main(int argc, char* argv[])
{
    options_t options = {
        .cmdline = NULL,
        .console_device = "/dev/tty0",
    };

    if (argc > 1)
    {
        parse_cmdline(&options, argv[1]);
    }

    int fd = open(options.console_device, O_RDWR, 0);
    if (fd != 0 || dup(0) || dup(0))
    {
        printf("cannot open console, fd = %d\n", fd);
        return EXIT_FAILURE;
    }

    printf(RED "W" GREEN "e" YELLOW "l" BLUE "c" MAGENTA "o" CYAN "m" MAGENTA "e!\n" RESET);
    printf(BG"Have fun..."RESET"\n");
    printf("\e[38;2;35;135;39m...with colors :)"RESET"\n");

    if (shell_run())
    {
        printf("cannot run shell\n");
        return EXIT_FAILURE;
    }

    return 0;
}
