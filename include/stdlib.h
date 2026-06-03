#pragma once
#include <stddef.h>
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t n, size_t size);
void *realloc(void *ptr, size_t size);
int atoi(const char *s);
double atof(const char *s);
int abs(int value);
void exit(int status);
char *getenv(const char *name);
int system(const char *command);
