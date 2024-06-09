#include "kernel/stat.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <kernel/reboot.h>

#define MAX_CMDLINE_LEN 128

typedef struct options
{
    const char* cmdline;
    char console_device[MAX_CMDLINE_LEN];
} options_t;

static inline void parse_cmdline(options_t* options, const char* cmdline)
{
    char* token;
    char* temp = malloc(strlen(cmdline) + 1);
    strcpy(temp, cmdline);

    options->cmdline = cmdline;

    token = strtok(temp, " ");

    while (token)
    {
        if (!strncmp(token, "console=", 8))
        {
            if (!(token = strtok(token + 8, " ")))
            {
                abort();
            }
            strcpy(options->console_device, token);
        }
        token = strtok(NULL, " ");
    }
}

const char* shell_find(void)
{
    const char* shells[] = {"/bin/bash", "/bin/sh"};
    struct stat buf;

    for (size_t i = 0; i < sizeof(shells) / sizeof(*shells); ++i)
    {
        if (!stat(shells[i], &buf))
        {
            return shells[i];
        }
    }

    return NULL;
}

int shell_run()
{
    int child_pid, status;
    const char* pathname = shell_find();
    char* const argv[] = {(char*)pathname, NULL};

    if (!pathname)
    {
        printf("cannot find shell to run!\n");
        return EXIT_FAILURE;
    }

    if ((child_pid = fork()) < 0)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    else if (child_pid == 0)
    {
        if (setsid())
        {
            perror("setsid");
            return EXIT_FAILURE;
        }

        setenv("PATH", "/bin", 0);
        setenv("SHELL", pathname, 0);
        setenv("HOME", "/root", 0);

        if (execvp(pathname, argv))
        {
            perror(pathname);
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
