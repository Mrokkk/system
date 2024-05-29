#pragma once

#include <stddef.h>

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr); // FIXME: non-standard free

[[noreturn]] void exit(int retcode);
[[noreturn]] void abort(void);

char* getenv(const char* name);
char* secure_getenv(const char* name);

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    -1
