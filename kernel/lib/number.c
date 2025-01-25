#ifdef NUMBER_SUFFIX
#ifdef __number_func
#undef __number_func
#endif

#define __number_func(SUFFIX, TYPE) \
    static char* PASTE(number, SUFFIX)( \
        char* str, \
        TYPE num, \
        int base, \
        int size, \
        int precision, \
        int type, \
        const char* end)

__number_func(NUMBER_SUFFIX, NUMBER_TYPE)
{
    char tmp[66];
    char c, sign, locase;
    int i;

    // locase = 0 or 0x20. ORing digits or letters with 'locase'
    // produces same digits or (maybe lowercased) letters
    locase = (type & SMALL);
    if (type & LEFT)
    {
        type &= ~ZEROPAD;
    }
    if (base < 2 || base > 16)
    {
        return NULL;
    }

    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;

    if (type & SIGN)
    {
        if (num < 0)
        {
            sign = '-';
            num = -num;
            size--;
        }
        else if (type & PLUS)
        {
            sign = '+';
            size--;
        }
        else if (type & SPACE)
        {
            sign = ' ';
            size--;
        }
    }

    if (type & SPECIAL)
    {
        if (base == 16)
        {
            size -= 2;
        }
        else if (base == 8)
        {
            size--;
        }
    }

    i = 0;
    if (num == 0)
    {
        tmp[i++] = '0';
    }
    else
    {
        for (; num != 0; tmp[i++] = (digits[do_div(num, base)] | locase));
    }

    if (i > precision)
    {
        precision = i;
    }

    size -= precision;
    if (!(type & (ZEROPAD + LEFT)))
    {
        for (; size-- > 0; PUTC(' '));
    }

    if (sign)
    {
        PUTC(sign);
    }

    if (type & SPECIAL)
    {
        if (base == 8)
        {
            PUTC('0');
        }
        else if (base == 16)
        {
            PUTC('0');
            PUTC('X' | locase);
        }
    }

    if (!(type & LEFT))
    {
        for (; size-- > 0; PUTC(c));
    }

    while (i < precision--)
    {
        PUTC('0');
    }

    while (i-- > 0)
    {
        PUTC(tmp[i]);
    }

    while (size-- > 0)
    {
        PUTC(' ');
    }

    return str;
}
#endif
