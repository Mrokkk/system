#pragma once

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

__NORETURN void exit(int retcode);
__NORETURN void _exit(int retcode);
__NORETURN void _Exit(int retcode);
__NORETURN void abort(void);

int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
char* getenv(const char* name);
char* secure_getenv(const char* name);

int system(const char* command);

int atexit(void (*function)(void));

int atoi(const char* nptr);
long atol(const char* nptr);
double atof(const char* nptr);

long strtol(
    const char* __RESTRICT nptr,
    char** __RESTRICT endptr,
    int base);

long long strtoll(
    const char* __RESTRICT nptr,
    char** __RESTRICT endptr,
    int base);

unsigned long strtoul(
    const char* __RESTRICT nptr,
    char** __RESTRICT endptr,
    int base);

unsigned long long strtoull(
    const char* __RESTRICT nptr,
    char** __RESTRICT endptr,
    int base);

double strtod(const char* __RESTRICT nptr, char** __RESTRICT endptr);
float strtof(const char* __RESTRICT nptr, char** __RESTRICT endptr);
long double strtold(const char* nptr, char** endptr);

void qsort(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void*, const void*));

void qsort_r(
    void* base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void*, const void*, void *),
    void* arg);

void* bsearch(
    const void* key,
    const void* base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void*, const void*));

int rand(void);
void srand(unsigned int seed);

static __INLINE int abs(int j)
{
    return __builtin_abs(j);
}

static __INLINE long labs(long j)
{
    return __builtin_abs(j);
}

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    -1

#define MB_CUR_MAX      1

#define ATEXIT_MAX      32

typedef struct
{
    int quot; /* Quotient */
    int rem;  /* Remainder */
} div_t;

typedef struct
{
    long quot; /* Quotient */
    long rem;  /* Remainder */
} ldiv_t;

typedef struct
{
    long long quot; /* Quotient */
    long long rem;  /* Remainder */
} lldiv_t;

div_t div(int numerator, int denominator);
ldiv_t ldiv(long numerator, long denominator);
lldiv_t lldiv(long long numerator, long long denominator);

__END_DECLS
