#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "history.h"

#define CWD_LEN         256
#define LINE_LEN        256
#define ARGV_MAX_SIZE   16

int c_cd();
int c_fg();
int c_env();
int c_pwd();
int c_mkdir();
int c_export();
static void command_prompt_print(int status);

#define COMMAND(name) {#name, c_##name}

static struct {
    const char* name;
    int (*function)();
} commands[] = {
    COMMAND(cd),
    COMMAND(fg),
    COMMAND(env),
    COMMAND(pwd),
    COMMAND(mkdir),
    COMMAND(export),
    COMMAND(history),
    {0, 0}
};

struct job
{
    int pid;
    struct job* next;
};

static char cwd[256];

#define SYSTEM_ERROR    1
#define COMMAND_ERROR   2

void sigint(int)
{
    printf("^C\n");
}

void sigtstp(int)
{
    printf("^Z\n");
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
    int error = chdir(!arg || !arg[0] ? "/" : arg);
    if (error)
    {
        return SYSTEM_ERROR;
    }
    if (!getcwd(cwd, CWD_LEN))
    {
        return SYSTEM_ERROR;
    }
    setenv("PWD", cwd, 1);
    return 0;
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
        return SYSTEM_ERROR;
    }

    if (waitpid(pid, &status, WUNTRACED))
    {
        return SYSTEM_ERROR;
    }

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

int c_env()
{
    extern char** environ;
    for (int i = 0; environ[i]; ++i)
    {
        printf("%s\n", environ[i]);
    }
    return 0;
}

int c_pwd(const char*)
{
    puts(cwd);
    return 0;
}

int c_mkdir(const char* arg)
{
    return mkdir(arg, 0);
}

int c_export(const char* arg)
{
    if (!arg || !*arg)
    {
        printf("KEY=VALUE expected\n");
        return COMMAND_ERROR;
    }

    char name[256];
    size_t len = strlen(arg);
    size_t delim = strcspn(arg, "=");

    if (delim == len)
    {
        printf("KEY=VALUE expected\n");
        return COMMAND_ERROR;
    }

    strncpy(name, arg, delim);
    name[delim] = 0;

    if (setenv(name, arg + delim + 1, 1))
    {
        printf("Failed to set env variable\n");
        return COMMAND_ERROR;
    }

    return 0;
}

static void command_prompt_print(int status)
{
    printf("%s %c ", cwd, status ? '*' : '#');
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
                if (*status == SYSTEM_ERROR)
                {
                    perror(argv[1]);
                }
            }
            return 0;
        }
    }

    if (command[0] != '/')
    {
        sprintf(buffer, "/bin/%.120s", command);
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
        execvp(command, argv);
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

    const char* home = getenv("HOME");

    if (c_cd(home ? home : "/"))
    {
        perror("cannot set dir");
    }

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
