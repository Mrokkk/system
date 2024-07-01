#include <stdarg.h>
#include <kernel/vm.h>
#include <kernel/path.h>
#include <kernel/procfs.h>
#include <kernel/printk.h>
#include <kernel/process.h>
#include <kernel/seq_file.h>
#include <kernel/vm_print.h>

#define DEBUG_VM_PRINT_PAGES 0

#define MAX_COLUMNS 10

struct column
{
    const char* name;
    size_t      name_len;
    const char* fmt;
    size_t      len;
    char        separator;
};

typedef struct column column_t;

struct table_context
{
    column_t* column;
    char*     it;
    size_t    indent;
};

typedef struct table_context table_context_t;

struct table
{
    column_t columns[MAX_COLUMNS];
    size_t   count;
    void     (*print_line)(const char* line, void* data);
    int      (*row_fill)(table_context_t* ctx, void* data);
};

typedef struct table table_t;

#define TABLE_DECLARE(name, filler, printer) \
    table_t name = { \
        .row_fill = filler, \
        .print_line = printer, \
    }

static void table_column_add(table_t* t, const char* name, const char* fmt, size_t len, char sep)
{
    if (t->count == MAX_COLUMNS - 1)
    {
        return;
    }

    t->columns[t->count].name = name;
    t->columns[t->count].fmt = fmt;
    t->columns[t->count].len = len;
    t->columns[t->count].separator = sep;
    t->columns[t->count].name_len = strlen(name);
    t->count++;
}

static inline void spaces_fill(char** it, int count)
{
    for (int i = 0; i < count; ++i)
    {
        *(*it)++ = ' ';
    }
}

static void table_column_write(table_context_t* ctx, ...)
{
    va_list args;
    va_start(args, ctx);
    void* data = va_arg(args, void*);
    va_end(args);
    const char* start = ctx->it;
    int diff;

    ctx->it += sprintf(ctx->it, ctx->column->fmt, data);

    if ((diff = ctx->column->len - (ctx->it - start)) > 0)
    {
        spaces_fill(&ctx->it, diff);
    }

    *ctx->it++ = ctx->column->separator;
    *ctx->it = '\0';
    ctx->column++;
}

#define TABLE_NO_HEADER 32

static void table_print(table_t* t, int flag, void* data)
{
    char buffer[256];
    buffer[0] = 0;
    char* it = buffer;
    size_t indent = (flag & 0xf) * 2;

    if (!(flag & TABLE_NO_HEADER))
    {
        spaces_fill(&it, indent);
        for (size_t i = 0; i < t->count; ++i)
        {
            column_t* c = &t->columns[i];

            it += sprintf(it, "%s", c->name);

            if (c->len)
            {
                spaces_fill(&it, c->len - c->name_len);
                *it++ = ' ';
            }

            *it = '\0';
        }
        if (it != buffer)
        {
            t->print_line(buffer, data);
        }
    }

    for (;;)
    {
        it = buffer;

        spaces_fill(&it, indent);

        table_context_t ctx = {.column = t->columns, .it = it};

        if (t->row_fill(&ctx, data))
        {
            break;
        }
        t->print_line(buffer, data);
    }
}

void vm_file_path_read(vm_area_t* vma, char* buffer, size_t max_len)
{
    dentry_t* dentry;
    int type = VM_TYPE_GET(vma->vm_flags);

    if (vma->inode)
    {
        if ((dentry = dentry_get(vma->inode)))
        {
            path_construct(dentry, buffer, max_len);
        }
        else
        {
            __builtin_strcpy(buffer, "missing dentry");
        }
    }
    else
    {
        switch (type)
        {
            case VM_TYPE_HEAP:
                __builtin_strcpy(buffer, "[heap]");
                break;
            case VM_TYPE_STACK:
                __builtin_strcpy(buffer, "[stack]");
                break;
            default:
                *buffer = '\0';
        }
    }
}

static void vm_header_print(const printk_entry_t* entry, const char* fmt, va_list args)
{
    char buf[128];
    vsprintf(buf, fmt, args);
    printk(entry, buf);
}

struct vma_printer_data
{
    size_t                count;
    const printk_entry_t* entry;
    vm_area_t*            vma;
};

static void vm_area_columns_fill(table_t* table)
{
    table_column_add(table, "vma", "%08x", 10, ' ');
    table_column_add(table, "start", "%08x", 10, '-');
    table_column_add(table, "end", "%08x", 10, ' ');
    table_column_add(table, "size", "%08x", 10, ' ');
    table_column_add(table, "offset", "%08x", 10, ' ');
    table_column_add(table, "flag", "%s", 4, ' ');
    table_column_add(table, "refcount", "%d", 8, ' ');
    table_column_add(table, "name", "%s", 0, '\0');
}

static int vma_row_fill(table_context_t* ctx, void* data)
{
    struct vma_printer_data* d = data;
    vm_area_t* vma = d->vma;
    char buf[48];

    if (!vma || !d->count)
    {
        return 1;
    }

    table_column_write(ctx, vma);
    table_column_write(ctx, vma->start);
    table_column_write(ctx, vma->end);
    table_column_write(ctx, vma->end - vma->start);
    table_column_write(ctx, vma->offset);
    table_column_write(ctx, vm_flags_string(buf, vma->vm_flags));
    table_column_write(ctx, vma->pages->refcount);
    vm_file_path_read(vma, buf, 48);
    table_column_write(ctx, buf);
    d->vma = vma->next;
    d->count--;

    return 0;
}

static void vma_line_print(const char* line, void* data)
{
    struct vma_printer_data* d = data;
    printk(d->entry, "%s", line);
}

void vm_area_log_internal(const printk_entry_t* entry, vm_area_t* vma, int indent_lvl)
{
    TABLE_DECLARE(table, &vma_row_fill, &vma_line_print);
    vm_area_columns_fill(&table);

    struct vma_printer_data data = {
        .entry = entry,
        .vma = vma,
        .count = 1,
    };

    table_print(&table, indent_lvl, &data);
}

void vm_areas_log_internal(const printk_entry_t* entry,
    vm_area_t* vm_areas,
    int indent_lvl,
    const char* header_fmt,
    ...)
{
    TABLE_DECLARE(table, &vma_row_fill, &vma_line_print);
    vm_area_columns_fill(&table);

    va_list args;
    va_start(args, header_fmt);
    vm_header_print(entry, header_fmt, args);
    va_end(args);

    struct vma_printer_data data = {
        .entry = entry,
        .vma = vm_areas,
        .count = -1
    };

    table_print(&table, indent_lvl, &data);
}

struct maps_table_data
{
    seq_file_t* s;
    vm_area_t*  vma;
};

static void maps_line_print(const char* line, void* data)
{
    struct maps_table_data* d = data;
    seq_puts(d->s, line);
    seq_putc(d->s, '\n');
}

static inline char* vm_maps_flags_string(char* buffer, const vm_area_t* vma)
{
    char* b = buffer;
    const int vm_flags = vma->vm_flags;
    *b++ = (vm_flags & VM_READ) ? 'r' : '-';
    *b++ = (vm_flags & VM_WRITE) ? 'w' : '-';
    *b++ = (vm_flags & VM_EXEC) ? 'x' : '-';
    *b++ = (vma->pages->refcount == 1) ? 'p' : 's';
    *b = '\0';
    return buffer;
}

static int maps_row_fill(table_context_t* ctx, void* data)
{
    struct maps_table_data* d = data;
    vm_area_t* vma = d->vma;
    char buf[PATH_MAX];

    if (!vma)
    {
        return 1;
    }

    table_column_write(ctx, vma->start);
    table_column_write(ctx, vma->end);
    table_column_write(ctx, vm_maps_flags_string(buf, vma));
    table_column_write(ctx, vma->offset);
    table_column_write(ctx, vma->inode ? vma->inode->ino : 0);
    vm_file_path_read(vma, buf, PATH_MAX);
    table_column_write(ctx, buf);
    d->vma = vma->next;

    return 0;
}

int maps_show(seq_file_t* s)
{
    TABLE_DECLARE(table, &maps_row_fill, &maps_line_print);

    table_column_add(&table, "start", "%08x", 10, '-');
    table_column_add(&table, "end", "%08x", 10, ' ');
    table_column_add(&table, "prot", "%s", 4, ' ');
    table_column_add(&table, "offset", "%08x", 10, ' ');
    table_column_add(&table, "ino", "%d", 10, ' ');
    table_column_add(&table, "name", "%s", 0, '\0');

    process_t* p = process_get(s);

    if (unlikely(!p))
    {
        return -ESRCH;
    }

    struct maps_table_data data = {
        .s = s,
        .vma = p->mm->vm_areas,
    };

    table_print(&table, TABLE_NO_HEADER | INDENT_LVL_0, &data);

    return 0;
}
