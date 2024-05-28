#pragma once

#include <stddef.h>
#include <stdint.h>

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void free(void* ptr); // FIXME: non-standard free

[[noreturn]] void exit(int retcode);
[[noreturn]] void abort(void);

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    -1
