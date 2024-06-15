#pragma once

#include <unistd.h>

#include "file.h"
#include "printf_buffer.h"

static inline int __flush_if_needed(printf_buffer_t* buffer)
{
    if (buffer->current == buffer->end)
    {
        if (UNLIKELY(file_flush(FILE_FROM_BUFFER(buffer))))
        {
            return PRINTF_ERROR_SYSTEM;
        }
    }

    return PRINTF_NO_ERROR;
}

static inline int __buffer_putc(printf_buffer_t* buffer, char c)
{
    *buffer->current++ = c;
    return 0;
}

static inline int __flush_if_newline(printf_buffer_t* buffer, char c)
{
    if (c == '\n')
    {
        if (UNLIKELY(file_flush(FILE_FROM_BUFFER(buffer))))
        {
            return PRINTF_ERROR_SYSTEM;
        }
    }
    return PRINTF_NO_ERROR;
}

static printf_error_t file_putc_lbf(printf_buffer_t* buffer, char c)
{
    if (__flush_if_needed(buffer) ||
        __buffer_putc(buffer, c) ||
        __flush_if_newline(buffer, c))
    {
        return PRINTF_ERROR_SYSTEM;
    }

    return PRINTF_NO_ERROR;
}

static printf_error_t file_putc_fbf(printf_buffer_t* buffer, char c)
{
    if (__flush_if_needed(buffer) ||
        __buffer_putc(buffer, c))
    {
        return PRINTF_ERROR_SYSTEM;
    }

    return PRINTF_NO_ERROR;
}

static printf_error_t file_putc_nbf(printf_buffer_t* buffer, char c)
{
    if (__buffer_putc(buffer, c) ||
        file_flush(FILE_FROM_BUFFER(buffer)))
    {
        return PRINTF_ERROR_SYSTEM;
    }

    return PRINTF_NO_ERROR;
}
