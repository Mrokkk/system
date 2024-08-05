#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/compiler.h>

int bitflags_string_impl(char* buffer, size_t size, const char* strings[], uint32_t bitflags, size_t n);

#define bitflags_string(buffer, n, strings, bitflags) \
    bitflags_string_impl(buffer, n, strings, bitflags, array_size(strings))
