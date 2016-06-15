#include <kernel/string.h>

/*
 * Kernel uses these if arch-dependent part doesn't define them
 */

/*===========================================================================*
 *                                   strcmp                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_STRCMP
int strcmp(const char *string1, const char *string2) {

    if (string1 == 0 || string2 == 0) return 1;

    while (1) {
        if (*string1++ != *string2++)
            return 1;
        if (*string1 == '\0' && *string2 == '\0')
            return 0;
        if (*string1 == '\0' || *string2 == '\0')
            return 1;
    }

    return 0;

}
#endif

/*===========================================================================*
 *                                  strncmp                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_STRNCMP
int strncmp(const char *cs, const char *ct, size_t count) {

    register signed char __res = 0;

    while (count) {
        if ((__res = *cs - *ct++) != 0 || !*cs++)
            break;
        count--;
    }

    return __res;
}
#endif

/*===========================================================================*
 *                                  strrchr                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_STRRCHR
char *strrchr(const char *string, int c) {

    int i, len, last = -1;

    if (string == 0) return 0;

    len = strlen(string);

    for (i=0; i<len; i++) {
        if (string[i] == (char)c)
            last = i;
    }

    if (last != -1) return (char *)&string[last];

    return 0;

}
#endif

/*===========================================================================*
 *                                   strlen                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_STRLEN
unsigned int strlen(const char *string) {

    char *temp;
    for (temp=(char *)string; *temp!=0; temp++);
    return temp-string;

}
#endif

/*===========================================================================*
 *                                  strnlen                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_STRNLEN
unsigned int strnlen(const char *s, unsigned int maxlen) {

    register const char *e;
    unsigned int n;

    for (e = s, n = 0; *e && n < maxlen; e++, n++)
        ;

    return n;

}
#endif

/*===========================================================================*
 *                                   strcpy                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_STRCPY
char *strcpy(char *dest, const char *src) {

    while ((*dest++ = *src++) != 0);

    return dest;

}
#endif

/*===========================================================================*
 *                                   strchr                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_STRCHR
char *strchr(const char *string, int c) {

    int i, len;

    if (string == 0) return 0;

    len = strlen(string);

    for (i=0; i<len; i++) {
        if (string[i] == (char)c)
            return (char *)&string[i];
    }

    return 0;

}
#endif

/*===========================================================================*
 *                                   memcpy                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_MEMCPY
void *memcpy(void *dest, const void *src, unsigned int size) {

    unsigned int size4;
    unsigned int *d4, *s4;
    unsigned char *d1, *s1;

    for (size4 = size >> 2, d4 = (unsigned int *)dest, s4 = (unsigned int *)src;
         size4>0;
         size4--, *d4++ = *s4++
    );

    for (size = size % 4, d1 = (unsigned char *)d4, s1 = (unsigned char *)s4;
         size>0;
         size--, *d1++ = *s1++
    );

    return dest;

}
#endif

/*===========================================================================*
 *                                  memcpyw                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_MEMCPYW
void *memcpyw(unsigned short *dest, const unsigned short *src, unsigned int count) {

    unsigned short *s, *d;

    for (s = (unsigned short *)src, d = (unsigned short *)dest;
         count != 0;
         count--, *d++ = *s++
    );

    return dest;

}
#endif

/*===========================================================================*
 *                                   memset                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_MEMSET
void *memset(void *ptr, int c, unsigned int size) {

    unsigned int i;

    if (ptr == 0) return 0;

    for (i=0; i<size; i++)
        ((char *)ptr)[i] = c;

    return ptr;

}
#endif

/*===========================================================================*
 *                                  memsetw                                  *
 *===========================================================================*/
#ifndef __HAVE_ARCH_MEMSETW
void *memsetw(unsigned short *dest, unsigned short val, unsigned int count) {

    unsigned short *temp;

    for (temp = (unsigned short *)dest;
         count != 0;
         count--, *temp++ = val
    );

    return dest;

}
#endif
