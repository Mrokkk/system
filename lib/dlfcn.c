#include <dlfcn.h>
#include <stdlib.h>

void* LIBC(dlopen)(const char* filename, int flags)
{
    NOT_IMPLEMENTED(NULL, "%s, %#x", filename, flags);
}

int LIBC(dlclose)(void* handle)
{
    NOT_IMPLEMENTED(-1, "%p", handle);
}

void* LIBC(dlsym)(void* handle, const char* symbol)
{
    NOT_IMPLEMENTED(NULL, "%p, %s", handle, symbol);
}

LIBC_ALIAS(dlopen);
LIBC_ALIAS(dlclose);
LIBC_ALIAS(dlsym);
