#pragma once

// https://pubs.opengroup.org/onlinepubs/9699919799.orig/basedefs/locale.h.html

#define LC_CTYPE            0
#define LC_NUMERIC          1
#define LC_TIME             2
#define LC_COLLATE          3
#define LC_MONETARY         4
#define LC_MESSAGES         5
#define LC_ALL              6
#define LC_PAPER            7
#define LC_NAME             8
#define LC_ADDRESS          9
#define LC_TELEPHONE        10
#define LC_MEASUREMENT      11
#define LC_IDENTIFICATION   12

typedef struct locale* locale_t;

struct lconv
{
    char*   currency_symbol;
    char*   decimal_point;
    char    frac_digits;
    char*   grouping;
    char*   int_curr_symbol;
    char    int_frac_digits;
    char    int_n_cs_precedes;
    char    int_n_sep_by_space;
    char    int_n_sign_posn;
    char    int_p_cs_precedes;
    char    int_p_sep_by_space;
    char    int_p_sign_posn;
    char*   mon_decimal_point;
    char*   mon_grouping;
    char*   mon_thousands_sep;
    char*   negative_sign;
    char    n_cs_precedes;
    char    n_sep_by_space;
    char    n_sign_posn;
    char*   positive_sign;
    char    p_cs_precedes;
    char    p_sep_by_space;
    char    p_sign_posn;
    char*   thousands_sep;
};

struct locale
{
    void*       locales[13];
    const char* names[13];
};

locale_t duplocale(locale_t locobj);
void freelocale(locale_t locobj);
struct lconv* localeconv(void);
locale_t newlocale(int category_mask, const char* locale, locale_t base);
char* setlocale(int category, const char* locale);
locale_t uselocale(locale_t);
