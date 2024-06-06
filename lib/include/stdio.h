#pragma once

#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#define BUFSIZ          0

#define EOF             (-1)

typedef struct file FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int fgetc(FILE* stream);
int getc(FILE* stream);
int getchar(void);

char* fgets(char* s, int size, FILE* restrict stream);

int ungetc(int c, FILE* stream);

int fputc(int c, FILE* stream);
int putc(int c, FILE* stream);
int putchar(int c);

int fputs(const char* restrict s, FILE* restrict stream);
int puts(const char* s);

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* restrict stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* restrict stream);

FILE* fopen(const char* restrict pathname, const char* restrict mode);
FILE* fdopen(int fd, const char* mode);
int fclose(FILE* stream);
int fileno(FILE* stream);

int scanf(const char* restrict format, ...);
int fscanf(FILE* restrict stream, const char* restrict format, ...);

int vscanf(const char* restrict format, va_list ap);
int vfscanf(FILE* restrict stream, const char* restrict format, va_list ap);

int vsprintf(char* buf, const char* fmt, va_list args);
int vfprintf(FILE* restrict stream, const char* restrict format, va_list ap);
int sprintf(char* buf, const char* fmt, ...);
int fprintf(FILE* file, const char* fmt, ...);
int printf(const char* fmt, ...);

void perror(const char* s);

void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);

int fflush(FILE* stream);
