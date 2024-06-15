#pragma once

#include <stdarg.h>
#include <stddef.h>

typedef struct printf_buffer printf_buffer_t;
typedef enum buffer_type buffer_type_t;
typedef enum printf_error printf_error_t;

enum printf_error
{
    PRINTF_NO_ERROR,
    PRINTF_ERROR_OVERFLOW,
    PRINTF_ERROR_SYSTEM,
};

struct printf_buffer
{
    char*  start;
    char*  end;
    char*  current;
    void*  data;
    size_t written;

    printf_error_t (*putc)(printf_buffer_t* buffer, char c);
};

int vsprintf_internal(printf_buffer_t* buffer, const char* fmt, va_list args);
