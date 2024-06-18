#pragma once

#include <stdio.h>

size_t __fbufsize(FILE* stream);
size_t __fpending(FILE* stream);
int __flbf(FILE* stream);
int __freadable(FILE* stream);
int __fwritable(FILE* stream);
int __freading(FILE* stream);
int __fwriting(FILE* stream);
int __fsetlocking(FILE* stream, int type);
void _flushlbf(void);
void __fpurge(FILE* stream);
