#define log_fmt(fmt) "ksyms: " fmt
#include <kernel/ctype.h>
#include <kernel/ksyms.h>
#include <kernel/page_alloc.h>
#include <kernel/page_types.h>

static ksym_t* kernel_symbols;

static const char* next_word_read(const char* string, const char** output, size_t* size)
{
    const char* start = string;

    while (*string != '\n' && *string != '\0' && *string != ' ')
    {
        string++;
    }

    string++;

    *size = string - start;
    *output = start;

    return string;
}

static uintptr_t hex_to_native(const char* s, size_t len)
{
    uintptr_t result = 0;
    int c;

    while (len--)
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

static inline int ksym_address(uintptr_t address, const ksym_t* ksym)
{
    return (address <= ksym->address + ksym->size) && address >= ksym->address;
}

ksym_t* ksym_find(uintptr_t address)
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

void ksym_string(uintptr_t addr, char* buffer, size_t size)
{
    ksym_t* symbol = ksym_find(addr);

    if (symbol)
    {
        snprintf(buffer, size, "[<%p>] %s+%zx/%zx",
            ptr(addr),
            symbol->name,
            (size_t)(addr - symbol->address),
            (size_t)symbol->size);
    }
    else
    {
        snprintf(buffer, size, "[<%p>] unknown", ptr(addr));
    }
}

static size_t ksyms_count_get(const char* symbols, const char* end)
{
    const char* temp = symbols;
    size_t count = 0;

    while (temp < end)
    {
        if (*temp++ == '\n') count++;
    }

    return count;
}

static int ksym_read(const char** symbols, char** buf_start, char** buf, ksym_t** prev_symbol)
{
    ksym_t* symbol = NULL;
    size_t len;
    size_t str_type_len = 0;
    size_t str_size_len = 0;
    size_t str_address_len = 0;
    size_t name_len = 0;
    const char* str_type;
    const char* str_size;
    const char* str_address;
    const char* name;

    *symbols = next_word_read(*symbols, &str_address, &str_address_len);

    if (!isalpha(**symbols))
    {
        *symbols = next_word_read(*symbols, &str_size, &str_size_len);
    }

    *symbols = next_word_read(*symbols, &str_type, &str_type_len);
    *symbols = next_word_read(*symbols, &name, &name_len);

    // Ignore symbols on rodata and symbols w/o size
    if (*str_type == 'r' || !str_size_len)
    {
        return 0;
    }

    if (unlikely(!str_type_len || !str_size_len || !str_address_len))
    {
        log_error("incorrect format of kernel symbols");
        return -1;
    }

    len = align(sizeof(ksym_t) + name_len, sizeof(uintptr_t));

    if (*buf - *buf_start + len > PAGE_SIZE)
    {
        if (!(*buf = *buf_start = single_page()))
        {
            log_error("cannot allocate page for next symbols");
            return -ENOMEM;
        }
    }

    symbol = ptr(*buf);
    symbol->address = hex_to_native(str_address, str_address_len - 1);
    symbol->type = *str_type;
    symbol->size = hex_to_native(str_size, str_size_len - 1);
    symbol->next = NULL;
    memcpy(symbol->name, name, name_len - 1);
    symbol->name[name_len - 1] = 0;

    *buf += len;

    *prev_symbol ? (*prev_symbol)->next = symbol : 0;
    *prev_symbol = symbol;

    return 0;
}

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
    ksym_t* symbols;

    if (!(symbols = ptr(buf = buf_start = single_page())))
    {
        log_error("no mem for syms");
        goto finish;
    }

    const char* data = start;
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
        pages_free(page(phys_addr(addr)));
    }
    kernel_symbols = symbols;

    return errno;
}
