#include <locale.h>
#include <string.h>

static struct lconv default_lconv = {
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

static char c_locale_string[2] = "C";

struct lconv* LIBC(localeconv)(void)
{
    return &default_lconv;
}

char* LIBC(setlocale)(int, const char* locale)
{
    if (locale == NULL)
    {
        return c_locale_string;
    }

    return !strcmp(locale, "POSIX") || !strcmp(locale, "C") || !strcmp(locale, "")
        ? c_locale_string
        : NULL;
}

LIBC_ALIAS(localeconv);
LIBC_ALIAS(setlocale);
