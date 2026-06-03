#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

void *malloc(size_t size)
{
    return kmalloc(size);
}

void free(void *ptr)
{
    kfree(ptr);
}

void *calloc(size_t n, size_t size)
{
    size_t total = n * size;
    void *ptr = malloc(total);
    memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    return krealloc(ptr, size);
}

int atoi(const char *s)
{
    int sign = 1;
    int value = 0;
    while (*s == ' ') s++;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s++ - '0');
    }
    return value * sign;
}

double atof(const char *s)
{
    int sign = 1;
    double value = 0.0;
    double frac = 0.1;
    while (*s == ' ') s++;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10.0 + (*s++ - '0');
    }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            value += (*s++ - '0') * frac;
            frac *= 0.1;
        }
    }
    return sign * value;
}

int abs(int value)
{
    return value < 0 ? -value : value;
}

void exit(int status)
{
    (void) status;
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

char *getenv(const char *name)
{
    (void) name;
    return NULL;
}

int system(const char *command)
{
    (void) command;
    return -1;
}
