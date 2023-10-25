#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "history.h"

#define LINE_LEN        256
#define ARGV_MAX_SIZE   16

int c_cd();
int c_fg();
int c_pwd();
int c_mkdir();
static void command_prompt_print(int status);

#define COMMAND(name) {#name, c_##name}

static struct {
    const char* name;
    int (*function)();
} commands[] = {
    COMMAND(cd),
    COMMAND(fg),
    COMMAND(pwd),
    COMMAND(mkdir),
    COMMAND(history),
    {0, 0}
};

struct job
{
    int pid;
    struct job* next;
};

int sigint(int)
{
    printf("^C\n");
    return 0;
}

int sigtstp(int)
{
    printf("^Z\n");
    return 0;
}

static inline void read_line(char* line, size_t size)
{
    size = read(0, line, size);
    if (size == 0)
    {
        exit(0);
    }
    line[size - 1] = 0;
}

static inline const char* word_read(const char* string, char* output)
{
    while (*string != '\n' && *string != '\0' && *string != ' ')
    {
        *output++ = *string++;
    }

    if (*string)
    {
        string++;
    }
    *output = 0;

    return string;
}

int c_cd(const char* arg)
{
    if (!arg || !arg[0])
    {
        chdir("/");
        return 0;
    }
    return chdir(arg);
}

int c_fg(const char* arg)
{
    if (!arg || !arg[0])
    {
        return -1;
    }

    int err, status;
    int pid = arg[0] - '0';

    if ((err = kill(pid, SIGCONT)))
    {
        return err;
    }

    waitpid(pid, &status, WUNTRACED);

    if (WIFSTOPPED(status))
    {
        printf("Stopped\n");
    }
    else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV)
    {
        printf("Segmentation fault\n");
    }

    return 0;
}

int c_pwd(const char*)
{
    char buf[32];
    int errno;

    if ((errno = getcwd(buf, 32)))
    {
        return errno;
    }

    printf("%s\n", buf);
    return 0;
}

int c_mkdir(const char* arg)
{
    return mkdir(arg, 0);
}

static void command_prompt_print(int status)
{
    char cwd[128];
    getcwd(cwd, 128);
    printf(cwd);
    status ? printf(" * ") : printf(" # ");
}

static inline void cmd_args_read(char* command, int* argc, char* argv[], char* buffer[], const char* line)
{
    int i;
    const char* temp = word_read(line, command);

    for (*argc = 0; *argc < ARGV_MAX_SIZE && *temp; ++*argc)
    {
        temp = word_read(temp, buffer[*argc]);
    }

    argv[0] = command;
    for (i = 0; i < *argc; argv[i + 1] = buffer[i], ++i);
    argv[i + 1] = 0;
}

static inline int execute(const char* command, int argc, char* argv[], int* status)
{
    int pid, err;
    char buffer[128];
    struct stat s;
    (void)argc;

    for (int i = 0; commands[i].name; ++i)
    {
        if (!strcmp(commands[i].name, command))
        {
            if ((*status = commands[i].function(argv[1])))
            {
                perror(argv[1]);
            }
            return 0;
        }
    }

    if (command[0] != '/')
    {
        sprintf(buffer, "/bin/%s", command);
        err = stat(buffer, &s);
        if (err)
        {
            perror(command);
            *status = 1;
            return 0;
        }
        command = buffer;
    }

    if ((pid = fork()) == 0)
    {
        exec(command, argv);
        perror(command);
        exit(errno);
    }
    else if (pid < 0)
    {
        printf("%s: fork error!\n", command);
        *status = 1;
        return 1;
    }

    waitpid(pid, status, WUNTRACED);

    if (WIFSTOPPED(*status))
    {
        printf("Stopped\n");
    }
    else if (WIFSIGNALED(*status) && WTERMSIG(*status) == SIGSEGV)
    {
        printf("Segmentation fault\n");
    }

    return 1;
}

int main()
{
    char line[LINE_LEN];
    char command[LINE_LEN];
    int argc, status = 0;
    char* arguments[ARGV_MAX_SIZE];
    char* argv[ARGV_MAX_SIZE];

    if (setsid())
    {
        return EXIT_FAILURE;
    }

    signal(SIGINT, sigint);
    signal(SIGTSTP, sigtstp);

    history_initialize();

    for (argc = 0; argc < ARGV_MAX_SIZE; arguments[argc++] = malloc(32));

    while (1)
    {
        command_prompt_print(status);

        read_line(line, LINE_LEN);

        if (line[0] == 0 || line[0] == '#')
        {
            continue;
        }

        cmd_args_read(command, &argc, argv, arguments, line);
        execute(command, argc, argv, &status);
        history_add(line);
    }

    return 0;
}
