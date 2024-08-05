#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <kernel/compiler.h>

// snprintf - format string with boundary checking
//
// @buf - output buffer
// @size - size of the output buffer
// @fmt - format string
// @... - variadic arguments required by format string
//
// Unlike the standard snprintf, it will never return value bigger than size or value lower than 0
int snprintf(char* buf, size_t size, const char* fmt, ...) FORMAT(printf, 3, 4);

// csnprintf - format string with boundary checking by pointer to buffer end
//
// @buf - output buffer
// @end - pointer to the buffer end
// @fmt - format string
// @... - variadic arguments required by format string
//
// Function will return pointer to the next byte past the written range. This allows chaining calls
// to csnprintf in following manner:
//
// char buf[32];
// char* it = buf;
// const char* end = buf + sizeof(buf);
// it = csnprintf(it, end, "string1");
// it = csnprintf(it, end, " string2");
// it = csnprintf(it, end, " string3");
char* csnprintf(char* buf, const char* end, const char* fmt, ...) FORMAT(printf, 3, 4);

// vsnprintf - format string with boundary checking
//
// @buf - output buffer
// @size - size of the buffer
// @fmt - format string
// @args - variadic arguments list
//
// The same as snprintf but for va_list. Unlike the standard snprintf, it will never return value
// bigger than size. Neither would it return value lower than 0
int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) FORMAT(printf, 3, 0);

// ssnprintf - format string with boundary checking and return original pointer to buffer
//
// @buf - output buffer, which is also returned
// @size - size of the output buffer
// @fmt - format string
// @... - variadic arguments required by format string
//
// This is the same as snprintf, but instead of returning number of written characters, it returns
// buf. It can be used in following manner:
//
// char buffer[32];
// log_info("some value: %s", ssnprintf(buffer, sizeof(buffer), "%u", 23));
char* ssnprintf(char* buf, size_t size, const char* fmt, ...) FORMAT(printf, 3, 4);
