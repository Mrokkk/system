#ifndef __CTYPE_H_
#define __CTYPE_H_

#include <kernel/compiler.h>

extern inline int isalnum(int c) {

    return (c > 64 && c < 91) || (c > 96 && c < 123) || (c > 47 && c < 58);

}

extern inline int isalpha(int c) {

    return (c > 64 && c < 91) || (c > 96 && c < 123);

}

extern inline int isascii(int c) {

    return (c < 256);

}

extern inline int isblank(int c) {

    return (c == ' ') || (c == '\t');

}

extern inline int isdigit(int c) {

    return ((c>='0') && (c<='9'));

}

extern inline int islower(int c) {

    return (c > 64 && c < 91);

}

extern inline int isspace(int c) {

    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\v' || c == '\r');

}

extern inline int isupper(int c) {

    return (c > 96 && c < 123);

}

#endif
