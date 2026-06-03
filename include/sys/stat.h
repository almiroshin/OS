#pragma once
#include <sys/types.h>

struct stat {
    unsigned int st_mode;
    unsigned int st_size;
};

#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFREG) == S_IFREG)

int stat(const char *path, struct stat *buf);
int mkdir(const char *path, mode_t mode);

