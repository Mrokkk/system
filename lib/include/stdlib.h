#pragma once

#include <stddef.h>
#include <stdint.h>

void* malloc(size_t size);
void free(void* ptr, size_t size); // FIXME: non-standard free
