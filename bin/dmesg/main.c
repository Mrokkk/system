#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common/compiler.h>

#define BUFFER_SIZE 16 * 1024

#define RED          "\033[31m"
#define GREEN        "\033[32m"
#define YELLOW       "\033[33m"
#define BLUE         "\033[34m"
#define MAGENTA      "\033[35m"
#define CYAN         "\033[36m"
#define BOLD_RED     "\033[91m"
#define BOLD_GREEN   "\033[92m"
#define BOLD_YELLOW  "\033[93m"
#define BOLD_BLUE    "\033[94m"
#define BOLD_MAGENTA "\033[95m"
#define BOLD_CYAN    "\033[96m"
#define CLEAR        "\033[0m"

static const char* loglevel_color[] = {
    [0] = CLEAR,
    [1] = "\033[38;5;245m",
    [2] = BLUE,
    [3] = BLUE,
    [4] = YELLOW,
    [5] = RED,
    [6] = RED,
};

static size_t syslog_read(char** data)
{
    const char* filename = "/proc/syslog";
    int fd = open(filename, O_RDONLY, 0);

    if (UNLIKELY(fd == -1))
    {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    int size;
    size_t total_size = 0;
    char* buf = NULL;

    do
    {
        buf = realloc(buf, total_size + BUFFER_SIZE);

        if (UNLIKELY(!buf))
        {
            perror(filename);
            exit(EXIT_FAILURE);
        }

        size = read(fd, buf + total_size, BUFFER_SIZE);

        if (size < 0)
        {
            perror(filename);
            exit(EXIT_FAILURE);
        }

        total_size += size;
    }
    while (size == BUFFER_SIZE);

    *data = buf;

    return total_size;
}

static void line_print(char* line, int line_nr)
{
    if (*line == ' ') // Continuation
    {
        fputs(line + 1, stdout);
    }
    else // New line
    {
        if (line_nr > 0)
        {
            fputc('\n', stdout);
        }

        char* semicolon = strchr(line, ';');
        unsigned loglevel = *line - '0';

        if (UNLIKELY(loglevel > 6))
        {
            fprintf(stderr, RED"Incorrect loglevel in line: "CLEAR);
            fputs(line, stderr);
            fputc('\n', stderr);
            return;
        }

        *semicolon = 0;

        char* save_ptr;
        strtok_r(line + 2, ",", &save_ptr);
        char* ts = strtok_r(NULL, ",", &save_ptr);

        printf(GREEN "[%14s] %s", ts, loglevel_color[loglevel]);

        fputs(semicolon + 1, stdout);
    }
}

int main(int, char*[])
{
    char* data = NULL;
    char* save_ptr;

    syslog_read(&data);

    char* tmp = data;

    for (int i = 0;; ++i, tmp = NULL)
    {
        char* line = strtok_r(tmp, "\n", &save_ptr);

        if (!line)
        {
            break;
        }

        line_print(line, i);
    }

    fputs(CLEAR "\n", stdout);
}
