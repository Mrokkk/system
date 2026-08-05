#pragma once

#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#define BUFSIZ          (1024 - 64)

#define _IONBF 0    /* unbuffered */
#define _IOLBF 1    /* line buffered */
#define _IOFBF 2    /* fully buffered */

#define EOF             (-1)

typedef struct file FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int setvbuf(FILE* __RESTRICT stream, char* buf, int mode, size_t size);

void setbuf(FILE* __RESTRICT stream, char* buf);
void setbuffer(FILE* __RESTRICT stream, char* buf, size_t size);
void setlinebuf(FILE* stream);

int fgetc(FILE* stream);
int getc(FILE* stream);
int getchar(void);

char* fgets(char* s, int size, FILE* __RESTRICT stream);

int ungetc(int c, FILE* stream);

int fputc(int c, FILE* stream);
int putc(int c, FILE* stream);
int putchar(int c);

int fputs(const char* __RESTRICT s, FILE* __RESTRICT stream);
int puts(const char* s);

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* __RESTRICT stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* __RESTRICT stream);

FILE* fopen(const char* __RESTRICT pathname, const char* __RESTRICT mode);
FILE* fdopen(int fd, const char* mode);
FILE* freopen(const char* __RESTRICT pathname, const char* __RESTRICT mode, FILE* __RESTRICT stream);

int fclose(FILE* stream);
int fileno(FILE* stream);

int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);

void rewind(FILE* stream);

int scanf(const char* __RESTRICT format, ...);
int fscanf(FILE* __RESTRICT stream, const char* __RESTRICT format, ...);

int vscanf(const char* __RESTRICT format, va_list ap);
int vfscanf(FILE* __RESTRICT stream, const char* __RESTRICT format, va_list ap);

int sscanf(const char* __RESTRICT str, const char* __RESTRICT format, ...);

int vsprintf(char* buf, const char* fmt, va_list args);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list args);
int vfprintf(FILE* __RESTRICT stream, const char* __RESTRICT format, va_list ap);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* str, size_t size, const char* __RESTRICT format, ...);
int fprintf(FILE* file, const char* fmt, ...);
int printf(const char* fmt, ...);
int vprintf(const char* __RESTRICT format, va_list ap);

void perror(const char* s);

void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fflush(FILE* stream);
int fpurge(FILE *stream);

int rename(const char* oldpath, const char* newpath);
int remove(const char* pathname);

#define P_tmpdir    "/tmp"
#define L_tmpnam    20
#define TMP_MAX     238328

char* tmpnam(char* s);

__END_DECLS
