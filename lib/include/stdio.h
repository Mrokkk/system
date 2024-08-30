#pragma once

#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#define BUFSIZ          (1024 - 64)

#define _IONBF 0    // unbuffered
#define _IOLBF 1    // line buffered
#define _IOFBF 2    // fully buffered

#define EOF             (-1)

typedef struct file FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int setvbuf(FILE* restrict stream, char* buf, int mode, size_t size);

static inline void setbuf(FILE* restrict stream, char* buf)
{
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

static inline void setbuffer(FILE* restrict stream, char* buf, size_t size)
{
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, size);
}

static inline void setlinebuf(FILE* stream)
{
    setvbuf(stream, NULL, _IOLBF, BUFSIZ);
}

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
FILE* freopen(const char* restrict pathname, const char* restrict mode, FILE* restrict stream);

int fclose(FILE* stream);
int fileno(FILE* stream);

int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);

void rewind(FILE* stream);

int scanf(const char* restrict format, ...);
int fscanf(FILE* restrict stream, const char* restrict format, ...);

int vscanf(const char* restrict format, va_list ap);
int vfscanf(FILE* restrict stream, const char* restrict format, va_list ap);

int sscanf(const char* restrict str, const char* restrict format, ...);

int vsprintf(char* buf, const char* fmt, va_list args);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list args);
int vfprintf(FILE* restrict stream, const char* restrict format, va_list ap);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* str, size_t size, const char* restrict format, ...);
int fprintf(FILE* file, const char* fmt, ...);
int printf(const char* fmt, ...);
int vprintf(const char* restrict format, va_list ap);

void perror(const char* s);

void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fflush(FILE* stream);
int fpurge(FILE *stream);

#define P_tmpdir    "/tmp"
#define L_tmpnam    20
#define TMP_MAX     238328

char* tmpnam(char* s);
