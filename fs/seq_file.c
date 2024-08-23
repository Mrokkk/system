#include <stdarg.h>
#include <kernel/minmax.h>
#include <kernel/seq_file.h>

#define DEBUG_SEQFILE 0

int seq_open(file_t* file, seq_show_t show)
{
    seq_file_t* s = alloc(seq_file_t);

    if (unlikely(!s))
    {
        return -ENOMEM;
    }

    log_debug(DEBUG_SEQFILE, "created seq_file=%x", s);

    s->file = file;
    s->size = 0;
    s->count = 0;
    s->buffer = NULL;
    s->private = NULL;
    s->show = show;

    file->private = s;

    return 0;
}

int seq_close(file_t* file)
{
    seq_file_t* s = file->private;

    log_debug(DEBUG_SEQFILE, "closing seq_file=%x", s);

    if (s->buffer)
    {
        page_free(s->buffer);
        page_free(ptr(addr(s->buffer) + PAGE_SIZE));
        page_free(ptr(addr(s->buffer) + 2 * PAGE_SIZE));
        page_free(ptr(addr(s->buffer) + 3 * PAGE_SIZE));
    }

    delete(s);

    return 0;
}

int seq_read(file_t* file, char* buffer, size_t count)
{
    int errno;
    size_t copy_count;
    seq_file_t* s = file->private;

    log_debug(DEBUG_SEQFILE, "seq_file=%x, count=%u", s, count);

    if (!s->buffer)
    {
        // FIXME: allocate pages dynamically
        page_t* pages = page_alloc(4, PAGE_ALLOC_CONT);

        if (unlikely(!pages))
        {
            return -ENOMEM;
        }

        s->buffer = page_virt_ptr(pages);
        s->size = 4 * PAGE_SIZE;

        if ((errno = s->show(s)))
        {
            return errno;
        }
    }

    copy_count = min(s->count - file->offset, count);

    if (copy_count == 0)
    {
        return 0;
    }

    log_debug(DEBUG_SEQFILE, "copying %u B", copy_count);

    memcpy(buffer, s->buffer + file->offset, copy_count);

    file->offset += copy_count;

    return copy_count;
}

int seq_printf(seq_file_t* s, const char* fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsnprintf(s->buffer + s->count, s->size - s->count, fmt, args);
    va_end(args);

    s->count += i;

    return i;
}

int seq_puts(seq_file_t* s, const char* string)
{
    size_t count = strlen(string);
    strcpy(s->buffer + s->count, string);
    s->count += count;
    return count;
}

int seq_putc(seq_file_t* s, const char c)
{
    *(s->buffer + s->count) = c;
    s->count += 1;
    return 1;
}
