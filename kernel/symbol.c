#include <kernel/kernel.h>

struct kernel_symbol *kernel_symbols;
int kernel_symbols_size;

char *word_read(char *string, char *output) {

    while(*string != '\n' && *string != '\0'
                && *string != ' ') {
        *output++ = *string++;
    }

    string++;
    *output = 0;

    return string;

}

unsigned int strhex2uint(char *s) {

    unsigned int result = 0;
    int c ;

    if ('0' == *s && 'x' == *(s+1)) {
        s+=2;
        while (*s) {
            result = result << 4;
            if (c = (*s - '0'), (c>=0 && c <=9)) result|=c;
            else if (c = (*s-'A'), (c>=0 && c <=5)) result|=(c+10);
            else if (c = (*s-'a'), (c>=0 && c <=5)) result|=(c+10);
            else break;
            ++s;
        }
    }
    return result;
}

char *symbol_read(char *temp, struct kernel_symbol *symbol) {

    char str_address[16], str_size[16],
         str_type[4], str_hex[16];
    char *temp2;

    temp = word_read(temp, symbol->name);
    temp = word_read(temp, str_address);
    temp = word_read(temp, str_size);
    temp = word_read(temp, str_type);

    sprintf(str_hex, "0x%s", str_address);
    symbol->address = strhex2uint(str_hex);
    symbol->type = *str_type;
    symbol->size = strtol(str_size, &temp2, 10);

    return temp;

}

struct kernel_symbol *symbol_find(const char *name) {

    int i;

    for (i = 0; i < kernel_symbols_size; i++) {
        if (!strncmp(kernel_symbols[i].name, name, 32))
            return &kernel_symbols[i];
    }

    return 0;

}

struct kernel_symbol *symbol_find_address(unsigned int address) {

    int i;

    for (i = 0; i < kernel_symbols_size; i++) {
        if ((address <= kernel_symbols[i].address + kernel_symbols[i].size)
                && (address >= kernel_symbols[i].address))
            return &kernel_symbols[i];
    }

    return 0;

}

int symbols_get_number(char *symbols, unsigned int size) {

    char *temp = symbols;
    int nr = 0;

    while ((unsigned int)temp < (unsigned int)symbols + size)
        if (*temp++ == '\n') nr++;

    return nr;

}

int symbols_read(char *symbols, unsigned int size) {

    int nr = 0, i = 0;

    nr = symbols_get_number(symbols, size);
    kernel_symbols = kmalloc(nr * sizeof(struct kernel_symbol));
    kernel_symbols_size = nr;

    for (i = 0; i<nr; i++)
        symbols = symbol_read(symbols, &kernel_symbols[i]);

    printk("Read %d symbols\n", nr);

    return 0;

}

