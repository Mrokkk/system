#pragma once

#include <stddef.h>
#include <stdint.h>

void* malloc(size_t size);
void* calloc(size_t size);
void free(void* ptr, size_t size); // FIXME: non-standard free

[[noreturn]] void abort(void);

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    -1
