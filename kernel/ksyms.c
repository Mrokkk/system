#define log_fmt(fmt) "ksyms: " fmt
#include <kernel/page.h>
#include <kernel/ctype.h>
#include <kernel/ksyms.h>

static ksym_t* kernel_symbols;

static uint32_t hex2uint32(const char* s)
{
    uint32_t result = 0;
    int c;

    while (*s)
    {
        result = result << 4;
        if (c = (*s - '0'), (c >= 0 && c <= 9)) result |= c;
        else if (c = (*s - 'A'), (c >= 0 && c <= 5)) result |= (c + 10);
        else if (c = (*s - 'a'), (c >= 0 && c <= 5)) result |= (c + 10);
        else break;
        ++s;
    }
    return result;
}

ksym_t* ksym_find_by_name(const char* name)
{
    for (ksym_t* symbol = kernel_symbols; symbol; symbol = symbol->next)
    {
        if (!strcmp(symbol->name, name))
        {
            return symbol;
        }
    }

    return NULL;
}

static inline int ksym_address(uint32_t address, ksym_t* ksym)
{
    return (address <= ksym->address + ksym->size) && address >= ksym->address;
}

ksym_t* ksym_find(uint32_t address)
{
    for (ksym_t* symbol = kernel_symbols; symbol; symbol = symbol->next)
    {
        if (ksym_address(address, symbol))
        {
            return symbol;
        }
    }

    return NULL;
}

void ksym_string(char* buffer, uint32_t addr)
{
    ksym_t* symbol = ksym_find(addr);

    if (symbol)
    {
        sprintf(buffer, "[<%08x>] %s+%x/%x",
            addr,
            symbol->name,
            addr - symbol->address,
            symbol->size);
    }
    else
    {
        sprintf(buffer, "[<%08x>] unknown", addr);
    }
}

static inline size_t ksyms_count_get(char* symbols, char* end)
{
    char* temp = symbols;
    size_t count = 0;

    while (temp < end)
    {
        if (*temp++ == '\n') count++;
    }

    return count;
}

static inline int ksym_read(char** symbols, char** buf_start, char** buf, ksym_t** prev_symbol)
{
    ksym_t* symbol = NULL;
    size_t len;
    char str_type[4];
    char str_size[16] = "\0";
    char str_address[12];
    char namebuf[32];

    *symbols = word_read(*symbols, str_address);
    if (!isalpha(**symbols))
    {
        *symbols = word_read(*symbols, str_size);
    }
    *symbols = word_read(*symbols, str_type);
    *symbols = word_read(*symbols, namebuf);

    // Ignore symbols on rodata and symbols w/o size
    if (*str_type == 'r' || !*str_size)
    {
        return 0;
    }

    len = align(sizeof(ksym_t) + strlen(namebuf) + 1, sizeof(uint32_t));

    if (*buf - *buf_start + len > PAGE_SIZE)
    {
        if (!(*buf = *buf_start = single_page()))
        {
            log_error("no mem for next syms");
            return -ENOMEM;
        }
    }

    symbol = ptr(*buf);
    symbol->address = hex2uint32(str_address);
    symbol->type = *str_type;
    symbol->size = hex2uint32(str_size);
    symbol->next = NULL;
    strcpy(symbol->name, namebuf);

    *buf += len;

    *prev_symbol ? (*prev_symbol)->next = symbol : 0;
    *prev_symbol = symbol;

    return 0;
}

struct memrange;
typedef struct memrange memrange_t;

struct memrange
{
    char* start;
    char* end;
};

#define MEMRANGE_INIT(s, e) \
    { .start = ptr(s), .end = ptr(e) }

#define memrange_for_each_page(it, r) \
    for (char* it = ptr(page_align(addr((r)->start))); it < (char*)(page_beginning(addr((r)->end))); it += PAGE_SIZE)

UNMAP_AFTER_INIT int ksyms_load(void* start, void* end)
{
    if (!start)
    {
        return 0;
    }

    int errno = -ENOMEM;
    char* buf_start;
    char* buf;
    size_t count = ksyms_count_get(start, end);
    memrange_t range = MEMRANGE_INIT(start, end);

    if (!(kernel_symbols = ptr(buf = buf_start = single_page())))
    {
        log_error("no mem for syms");
        goto finish;
    }

    char* data = start;
    ksym_t* prev_symbol = NULL;
    for (size_t i = 0; i < count; i++)
    {
        errno = ksym_read(&data, &buf_start, &buf, &prev_symbol);
        if (unlikely(errno))
        {
            goto finish;
        }
    }

    errno = 0;

finish:
    log_info("freeing [%x - %x]", range.start, page_beginning(addr(range.end)));
    memrange_for_each_page(addr, &range)
    {
        page_free(addr);
    }

    return errno;
}
