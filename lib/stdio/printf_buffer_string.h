#pragma once

#include "printf_buffer.h"

static printf_error_t string_putc_no_bound(printf_buffer_t* buffer, char c)
{
    *buffer->current++ = c;
    return 0;
}

static printf_error_t string_putc(printf_buffer_t* buffer, char c)
{
    if (UNLIKELY(buffer->current == buffer->end))
    {
        return PRINTF_ERROR_OVERFLOW;
    }

    *buffer->current++ = c;

    return PRINTF_NO_ERROR;
}
