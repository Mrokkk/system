#pragma once

#include <stddef.h>

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

[[noreturn]] void exit(int retcode);
[[noreturn]] void _exit(int retcode);
[[noreturn]] void _Exit(int retcode);
[[noreturn]] void abort(void);

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
    const char* restrict nptr,
    char** restrict endptr,
    int base);

long long strtoll(
    const char* restrict nptr,
    char** restrict endptr,
    int base);

unsigned long strtoul(
    const char* restrict nptr,
    char** restrict endptr,
    int base);

double strtod(const char* restrict nptr, char** restrict endptr);

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

static inline int abs(int j)
{
    return __builtin_abs(j);
}

static inline long labs(long j)
{
    return __builtin_abs(j);
}

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    -1

#define MB_CUR_MAX      1

#define ATEXIT_MAX      32
