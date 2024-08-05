#include <kernel/kernel.h>
#include <kernel/bitflag.h>

int bitflags_string_impl(char* buffer, size_t size, const char* strings[], uint32_t bitflags, size_t n)
{
    int len = 0;

    ASSERT(n <= sizeof(bitflags) * 8);

    for (size_t i = 0; i < n; i++)
    {
        if (strings[i] && (bitflags & (1 << i)))
        {
            if (len != 0)
            {
                buffer[len++] = ' ';
            }
            len += snprintf(buffer + len, size - len, "%s", strings[i]);
        }
    }

    return len;
}
