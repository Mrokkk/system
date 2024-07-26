#pragma once

#include <stddef.h>
#include <stdint.h>
#include <kernel/compiler.h>

int bitflags_string_impl(char* buffer, const char* strings[], uint32_t bitflags, size_t n);

#define bitflags_string(buffer, strings, bitflags) \
    bitflags_string_impl(buffer, strings, bitflags, array_size(strings))
