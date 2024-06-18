#include "file.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "printf_buffer_file.h"

static list_head_t files = LIST_INIT(files);

static_assert(sizeof(FILE) == 64);
static_assert(sizeof(FILE) + BUFSIZ == 1024);

int file_allocate(int fd, int mode, FILE** output)
{
    FILE* file;
    char* buffer;
    int flag = isatty(fd) ? _IOLBF : _IOFBF;

    file = SAFE_ALLOC(malloc(sizeof(*file) + BUFSIZ), -1);

    buffer = PTR(ADDR(file) + sizeof(*file));

    file->fd = fd;
    file->mode = mode;
    file->magic = FILE_MAGIC;
    file->last = 0;
    list_init(&file->files);

    file_setbuf(file, flag, buffer, BUFSIZ);

    list_add_tail(&file->files, &files);

    *output = file;

    return 0;
}

int file_flush(FILE* file)
{
    int res;
    res = write(file->fd, file->buffer.start, file->buffer.current - file->buffer.start);
    file->buffer.current = file->buffer.start;
    return res < 0 ? -1 : 0;
}

int file_flush_all(void)
{
    FILE* file;
    list_for_each_entry(file, &files, files)
    {
        fflush(file);
    }
    return 0;
}

int file_setbuf(FILE* file, int flag, char* buf, size_t bufsiz)
{
    switch (flag)
    {
        case _IOLBF:
            file->buffer.putc = &file_putc_lbf;
            break;
        case _IOFBF:
            file->buffer.putc = &file_putc_fbf;
            break;
        case _IONBF:
            file->buffer.putc = &file_putc_nbf;
            break;
        default:
            return -1;
    }

    file->buftype = flag;

    if (buf)
    {
        file->buffer.current = file->buffer.start = buf;
        file->buffer.end = buf + bufsiz;
        // TODO: free previos buffer
    }

    return 0;
}

int file_close(FILE* file)
{
    int fd = file->fd;

    fflush(file);
    list_del(&file->files);
    free(file);

    return close(fd);
}

int LIBC(fileno)(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), EOF);
    return stream->fd;
}

void LIBC(clearerr)(FILE* stream)
{
    if (stream)
    {
        stream->error = false;
    }
}

int LIBC(feof)(FILE*)
{
    return 0;
}

int LIBC(ferror)(FILE* stream)
{
    VALIDATE_INPUT(FILE_CHECK(stream), 1);
    return stream->error;
}

LIBC_ALIAS(clearerr);
LIBC_ALIAS(feof);
LIBC_ALIAS(ferror);
LIBC_ALIAS(fileno);
