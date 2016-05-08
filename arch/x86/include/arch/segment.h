#ifndef __SEGMENT_H_
#define __SEGMENT_H_

#define KERNEL_CS 0x8
#define KERNEL_DS 0x10
#define USER_CS 0x1b
#define USER_DS 0x23

#ifndef __ASSEMBLER__

extern inline unsigned char __get_user_byte(unsigned char *user) {
    unsigned char rv;
    asm volatile("movb %%gs:(%1), %0;" : "=r" (rv) : "r" (user));
    return rv;
}

extern inline unsigned short __get_user_word(unsigned short *user) {
    unsigned short rv;
    asm volatile("movw %%gs:(%1), %0;" : "=r" (rv) : "r" (user));
    return rv;
}

extern inline unsigned long __get_user_long(unsigned long *user) {
    unsigned long rv;
    asm volatile("movl %%gs:(%1), %0;" : "=r" (rv) : "r" (user));
    return rv;
}

extern inline void __put_user_byte(unsigned char val, unsigned char *user) {
    asm volatile("movb %1, %%gs:(%0);" :: "r" (val), "r" (user));
}

extern inline void __put_user_word(unsigned short val, unsigned short *user) {
    asm volatile("movw %1, %%gs:(%0);" :: "r" (val), "r" (user));
}

extern inline void __put_user_long(unsigned long val, unsigned long *user) {
    asm volatile("movl %1, %%gs:(%0);" :: "r" (val), "r" (user));
}

#define get_user_byte(address) \
    __get_user_byte((unsigned char *)(address))

#define get_user_word(address) \
    __get_user_word((unsigned short *)(address))

#define get_user_long(address) \
    __get_user_long((unsigned long *)(address))

#define put_user_byte(val, address) \
    __put_user_byte((unsigned char)(val), (unsigned char *)(address))

#define put_user_word(val, address) \
    __put_user_word((unsigned short)(val), (unsigned short *)(address))

#define put_user_long(val, address) \
    __put_user_long((unsigned long)(val), (unsigned long *)(address))

extern inline void *memcpy_from_user(void *dest, const void *src, unsigned int size) {

    register int dummy1, dummy2, dummy3;

    asm volatile(
            "cld;"
            "gs;"
            "rep;"
            "movsb;"
            : "=c" (dummy1), "=D" (dummy2), "=S" (dummy3)
            : "c" (size), "D" ((unsigned long)dest), "S" ((unsigned long)src)
            : "memory"
    );

    return dest;

}

extern inline void *memcpy_to_user(void *dest, const void *src, unsigned int size) {

    register int dummy1, dummy2, dummy3;

    asm volatile(
            "push %%es;"
            "push %%gs;"
            "pop %%es;"
            "cld;"
            "rep;"
            "movsb;"
            "pop %%es;"
            : "=c" (dummy1), "=D" (dummy2), "=S" (dummy3)
            : "c" (size), "D" ((unsigned long)dest), "S" ((unsigned long)src)
            : "memory"
    );

    return dest;

}

extern inline void strcpy_from_user(char *dest, const char *src) {

    while ((*dest++ = get_user_byte(src++)))
        ;

}

extern inline void strcpy_to_user(char *dest, const char *src) {

    do {
        put_user_byte(*src, dest++);
        if (!*src++) return;
    } while (1);

}

#endif

#endif /* __SEGMENT_H_ */
