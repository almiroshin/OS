#pragma once
#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int fprintf(FILE *stream, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int sprintf(char *str, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int sscanf(const char *str, const char *fmt, ...);
int puts(const char *s);
int putchar(int c);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int fflush(FILE *stream);
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);
