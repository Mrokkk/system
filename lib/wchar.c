#include <stddef.h>

typedef int wchar_t;

size_t wcslen(const wchar_t* s)
{
    const wchar_t* temp;
    for (temp = s; *temp != 0; temp++);
    return temp - s;
}

wchar_t* wcscat(wchar_t* restrict dest, const wchar_t* restrict src)
{
    wchar_t* old_dest = dest;
    dest += wcslen(dest);
    while ((*dest++ = *src++));
    *dest = 0;
    return old_dest;
}

int wctomb(char* s, wchar_t wc)
{
    NOT_IMPLEMENTED(-1, "%p, %x", s, wc);
}

wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t n)
{
    wchar_t* old_dest = dest;
    while (n--)
    {
        *dest++ = *src++;
    }
    return old_dest;
}

int wcwidth(wchar_t)
{
    return 1;
}
