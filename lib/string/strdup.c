#include <string.h>
#include <stdlib.h>

char* LIBC(strdup)(const char* s)
{
    VALIDATE_INPUT(s, NULL);

    size_t len = strlen(s) + 1;
    char* news = SAFE_ALLOC(malloc(len), NULL);

    memcpy(news, s, len);
    return news;
}

LIBC_ALIAS(strdup);
