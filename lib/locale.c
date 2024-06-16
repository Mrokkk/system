#include <locale.h>
#include <stdlib.h>

struct lconv default_lconv = {
    .currency_symbol = "",
    .decimal_point = ".",
    .frac_digits = 127,
    .grouping = "",
    .int_curr_symbol = "",
    .int_frac_digits = 127,
    .int_n_cs_precedes = 127,
    .int_n_sep_by_space = 127,
    .int_n_sign_posn = 127,
    .int_p_cs_precedes = 127,
    .int_p_sep_by_space = 127,
    .int_p_sign_posn = 127,
    .mon_decimal_point = "",
    .mon_grouping = "",
    .mon_thousands_sep = "",
    .negative_sign = "",
    .n_cs_precedes = 127,
    .n_sep_by_space = 127,
    .n_sign_posn = 127,
    .positive_sign = "",
    .p_cs_precedes = 127,
    .p_sep_by_space = 127,
    .p_sign_posn = 127,
    .thousands_sep = ""
};

locale_t LIBC(duplocale)(locale_t locobj)
{
    NOT_IMPLEMENTED(NULL, "%p", locobj);
}

void LIBC(freelocale)(locale_t locobj)
{
    NOT_IMPLEMENTED_NO_RET("%p", locobj);
}

struct lconv* LIBC(localeconv)(void)
{
    NOT_IMPLEMENTED_NO_ARGS(&default_lconv);
}

locale_t LIBC(newlocale)(int category_mask, const char* locale, locale_t base)
{
    NOT_IMPLEMENTED(NULL, "%d, \"%s\", %p", category_mask, locale, base);
}

char* LIBC(setlocale)(int category, const char* locale)
{
    NOT_IMPLEMENTED("C", "%d, \"%s\"", category, locale);
}

locale_t LIBC(uselocale)(locale_t locobj)
{
    NOT_IMPLEMENTED(NULL, "%p", locobj);
}

LIBC_ALIAS(duplocale);
LIBC_ALIAS(freelocale);
LIBC_ALIAS(localeconv);
LIBC_ALIAS(newlocale);
LIBC_ALIAS(setlocale);
LIBC_ALIAS(uselocale);
