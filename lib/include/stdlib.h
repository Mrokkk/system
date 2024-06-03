#pragma once

#include <stddef.h>

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

[[noreturn]] void exit(int retcode);
[[noreturn]] void abort(void);

int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
char* getenv(const char* name);
char* secure_getenv(const char* name);

int atoi(const char* nptr);

int atexit(void (*function)(void));

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    -1

#define MB_CUR_MAX      1
