#pragma once

#define asmlinkage

#define SYMBOL_NAME(x) x
#define SYMBOL_NAME_LABEL(x) x:
#define SYMBOL_NAME_STRING(x) #x

#ifdef __ASSEMBLER__

#define ALIGN(a) \
    .align a

#define ENTRY(name) \
    .global SYMBOL_NAME(name);  \
    ALIGN(4);                   \
    SYMBOL_NAME_LABEL(name)

#define END(name) \
    .size SYMBOL_NAME(name), .-SYMBOL_NAME(name);

#define ENDPROC(name) \
    .type SYMBOL_NAME(name), @function; \
    END(name)

#endif  // __ASSEMBLER__
