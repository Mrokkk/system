#include <arch/page.h>
#include <kernel/ctype.h>
#include <kernel/ksyms.h>

ksym_t* kernel_symbols;
int kernel_symbols_size;

typedef struct
{
    ksym_t* symbols;
    size_t ksyms_count;
    size_t ksyms_size;
} ksyms_t;

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

char* symbol_read(char* temp, ksym_t* symbol)
{
    char str_type[4];
    char str_size[16] = {'0', };
    char str_address[16];

    temp = word_read(temp, str_address);

    // Handling symbol w/o size
    if (temp && !isalpha(*temp))
    {
        temp = word_read(temp, str_size);
    }

    temp = word_read(temp, str_type);
    temp = word_read(temp, symbol->name);

    symbol->address = hex2uint32(str_address);
    symbol->type = *str_type;
    symbol->size = hex2uint32(str_size);

    return temp;
}

ksym_t* ksym_find_by_name(const char* name)
{
    for (int i = 0; i < kernel_symbols_size; ++i)
    {
        if (!strncmp(kernel_symbols[i].name, name, SYMBOL_NAME_SIZE))
        {
            return &kernel_symbols[i];
        }
    }

    return 0;
}

static inline int ksym_address(uint32_t address, ksym_t* ksym)
{
    return (address <= ksym->address + ksym->size) && address >= ksym->address;
}

ksym_t* ksym_find(uint32_t address)
{
    for (int i = 0; i < kernel_symbols_size; ++i)
    {
        if (ksym_address(address, kernel_symbols + i))
        {
            return &kernel_symbols[i];
        }
    }

    return 0;
}

static inline int ksyms_count_get(char* symbols, size_t size)
{
    char* temp = symbols;
    int count = 0;

    while (addr(temp) < addr(symbols) + size)
    {
        if (*temp++ == '\n') count++;
    }

    return count;
}

int ksyms_load(char* symbols, size_t size)
{
    int count;
    uint32_t required_size;
    uint32_t required_pages;

    count = ksyms_count_get(symbols, size);
    required_size = page_align(count * sizeof(ksym_t));
    required_pages = required_size / PAGE_SIZE;

    log_debug("got %u symbols; allocating %u B", count, count * sizeof(ksym_t));
    page_t* pages = page_alloc(required_pages, PAGE_ALLOC_CONT);

    if (unlikely(!pages))
    {
        log_exception("cannot allocate pages for ksyms!");
        return -1;
    }

    kernel_symbols = page_virt_ptr(pages);
    kernel_symbols_size = count;

    for (int i = 0; i < count; i++)
    {
        symbols = symbol_read(symbols, &kernel_symbols[i]);
    }

    log_debug("read %d symbols", count);

    return 0;
}
