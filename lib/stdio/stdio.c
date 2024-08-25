#include <stdio.h>

#include "file.h"

static FILE _stdin, _stdout, _stderr;
static char stdin_buffer[BUFSIZ], stdout_buffer[BUFSIZ], stderr_buffer[BUFSIZ];

FILE* stdin = &_stdin;
FILE* stdout = &_stdout;
FILE* stderr = &_stderr;

void stdio_init(void)
{
    file_init(STDIN_FILENO, O_RDONLY, stdin, stdin_buffer, sizeof(stdin_buffer));
    file_init(STDOUT_FILENO, O_WRONLY, stdout, stdout_buffer, sizeof(stdout_buffer));
    file_init(STDERR_FILENO, O_WRONLY, stderr, stderr_buffer, sizeof(stderr_buffer));
}

// Temporary
int LIBC(fsync)(int fd)
{
    NOT_IMPLEMENTED(0, "%d", fd);
}

LIBC_ALIAS(fsync);
