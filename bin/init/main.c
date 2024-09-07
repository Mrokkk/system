#include <time.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <common/compiler.h>
#include <kernel/api/reboot.h>

#define MAX_CMDLINE_LEN 128

typedef struct options
{
    const char* cmdline;
    char console_device[MAX_CMDLINE_LEN];
} options_t;

static void parse_cmdline(options_t* options, const char* cmdline)
{
    char* token;
    char* temp = malloc(strlen(cmdline) + 1);

    if (UNLIKELY(!temp))
    {
        __builtin_trap();
    }

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

static const char* shell_find(void)
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

static int shell_run(const char* pathname, const char* console_device)
{
    int child_pid;
    char* const argv[] = {(char*)pathname, NULL};

    setenv("PATH", "/bin", 0);
    setenv("SHELL", pathname, 0);
    setenv("HOME", "/root", 0);
    setenv("LC_ALL", "C", 0);
    setenv("TERM", "xterm-256color", 0);

    if ((child_pid = fork()) < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (child_pid == 0)
    {
        if (setsid())
        {
            perror("setsid");
            exit(EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);

        if ((open(console_device, O_RDONLY, 0) != STDIN_FILENO) ||
            (open(console_device, O_WRONLY, 0) != STDOUT_FILENO) ||
            (open(console_device, O_WRONLY, 0) != STDERR_FILENO))
        {
            syslog(LOG_ERR, "cannot open console for shell");
            exit(EXIT_FAILURE);
        }

        if (execvp(pathname, argv))
        {
            perror(pathname);
            exit(EXIT_FAILURE);
        }

        while (1);
    }

    return child_pid;
}

#define RED         "\e[31m"
#define GREEN       "\e[32m"
#define YELLOW      "\e[33m"
#define BLUE        "\e[34m"
#define MAGENTA     "\e[35m"
#define CYAN        "\e[36m"
#define BG          "\e[30;44m"
#define RESET       "\e[0m"

[[noreturn]] int main(int argc, char* argv[])
{
    options_t options = {
        .cmdline = NULL,
        .console_device = "/dev/tty0",
    };

    openlog("init", LOG_PID | LOG_CONS, LOG_USER);

    if (argc > 1)
    {
        parse_cmdline(&options, argv[1]);
    }

    if ((open(options.console_device, O_RDONLY, 0) != STDIN_FILENO) ||
        (open(options.console_device, O_WRONLY, 0) != STDOUT_FILENO))
    {
        syslog(LOG_ERR, "cannot open console");
        exit(EXIT_FAILURE);
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    printf(RED "W" GREEN "e" YELLOW "l" BLUE "c" MAGENTA "o" CYAN "m" MAGENTA "e!\n" RESET);
    printf(BG"Have fun..."RESET"\n");
    printf("\e[38;2;35;135;39m...with colors :)"RESET"\n");

    const char* shell = shell_find();

    time_t t = time(NULL);
    printf("%s", ctime(&t));

    if (!shell)
    {
        syslog(LOG_ERR, "cannot find shell to run");
        exit(EXIT_FAILURE);
    }

    int child_pid, status, rerun;

    while (1)
    {
        rerun = 0;

        child_pid = shell_run(shell, options.console_device);

        waitpid(child_pid, &status, 0);

        if (WIFSIGNALED(status))
        {
            printf("shell killed by %u", WTERMSIG(status));
        }
        else
        {
            printf("shell exited with %d", WEXITSTATUS(status));
        }

        int count;
        char tmp[128] = {0, };
        struct termios t;

        if (!tcgetattr(0, &t))
        {
            t.c_lflag |= ECHO | ICANON | ISIG | IEXTEN | ECHOE| ECHOKE | ECHOCTL;
            t.c_cc[VEOF] = 4;
            tcsetattr(0, 0, &t);
        }

        printf("; type sh to rerun, press CTRL+D to reboot\n");

        while (({ printf("> "); fflush(stdout); 1; }) && (count = read(STDIN_FILENO, tmp, 128)))
        {
            if (count > 0)
            {
                tmp[count - 1] = 0;
            }
            if (!strcmp(tmp, "sh"))
            {
                rerun = 1;
                break;
            }
            else
            {
                printf("unrecognized command: %s\n", tmp);
            }
        }

        if (!rerun)
        {
            printf("performing reboot\n");
            reboot(REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_CMD_RESTART);
        }
    }

    while (1);
}
