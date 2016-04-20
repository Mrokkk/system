#ifndef __LINKAGE_H_
#define __LINKAGE_H_

#define asmlinkage

#ifdef __ELF__

#define SYMBOL_NAME(x) x
#define SYMBOL_NAME_LABEL(x) x:
#define SYMBOL_NAME_STRING(x) #x

#else    /* __ELF__ */

#define SYMBOL_NAME(x) _##x
#ifdef __STDC__
#define SYMBOL_NAME_LABEL(x) _##x:
#else
#define SYMBOL_NAME_LABEL(x) _/**/x/**/:
#endif

#define SYMBOL_NAME_STRING(x) "_"#x

#endif    /* __ELF__ */

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

#endif    /* __ASSEMBLER__ */

#endif /* __LINKAGE_H_ */
