#include <locale.h>
#include <stdlib.h>

locale_t duplocale(locale_t locobj)
{
    UNUSED(locobj);
    return NULL;
}

void freelocale(locale_t locobj)
{
    UNUSED(locobj);
}

struct lconv* localeconv(void)
{
    return NULL;
}

locale_t newlocale(int category_mask, const char* locale, locale_t base)
{
    UNUSED(category_mask); UNUSED(locale); UNUSED(base);
    return NULL;
}

char* setlocale(int category, const char* locale)
{
    UNUSED(category); UNUSED(locale);
    return NULL;
}

locale_t uselocale(locale_t locobj)
{
    UNUSED(locobj);
    return NULL;
}
