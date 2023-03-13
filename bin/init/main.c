#include <stdio.h>
#include <stddef.h>
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
        return -1;
    }
    else if (child_pid == 0)
    {
        char* pathname = "/bin/sh";
        char* const argv[] = {pathname, NULL, };
        if (exec(pathname, argv))
        {
            printf("exec error in init!\n");
            return -1;
        }
        while (1);
    }

    waitpid(child_pid, &status, 0);

    printf("shell exited with %d, performing reboot\n", status);

    reboot(REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_CMD_RESTART);

    while (1);
}

#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define RESET       "\033[0m"

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
        return -1;
    }

    if (options.cmdline)
    {
        printf("cmdline: %s\n", options.cmdline);
        printf("console=%s\n", options.console_device);
    }

    printf(RED "W" GREEN "e" YELLOW "l" BLUE "c" MAGENTA "o" CYAN "m" MAGENTA "e!\n" RESET);

    if (shell_run())
    {
        printf("cannot run shell\n");
        return -1;
    }

    return 0;
}
