#pragma once

#include <kernel/fs.h>

struct seq_file;
typedef struct seq_file seq_file_t;

typedef int (*seq_show_t)(seq_file_t* seq_file);

struct seq_file
{
    page_t*    pages;
    char*      buffer;
    size_t     size;
    size_t     count;
    file_t*    file;
    seq_show_t show;
    void*      private;
};

int seq_open(file_t* file, seq_show_t show);
int seq_close(file_t* file);
int seq_read(file_t* file, char* buffer, size_t count);
int seq_printf(seq_file_t* s, const char* fmt, ...);
int seq_puts(seq_file_t* s, const char* string);
int seq_putc(seq_file_t* s, const char c);
