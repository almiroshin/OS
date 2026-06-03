#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fs.h"
#include "serial.h"

struct FILE {
    int kind;
    const unsigned char *data;
    unsigned char *write_data;
    size_t length;
    size_t pos;
    size_t capacity;
    char path[64];
};

static struct FILE console = { 1, NULL, NULL, 0, 0, 0, {0} };
static struct FILE ram_files[8];
FILE *stdin = &console;
FILE *stdout = &console;
FILE *stderr = &console;

static void out_char(char **buf, size_t *left, int c)
{
    if (buf && *buf && *left > 1) {
        **buf = (char)c;
        (*buf)++;
        (*left)--;
    } else if (!buf) {
        char s[2] = { (char)c, 0 };
        serial_write(s);
    }
}

static void out_str(char **buf, size_t *left, const char *s)
{
    if (!s) s = "(null)";
    while (*s) out_char(buf, left, *s++);
}

static void out_uint(char **buf, size_t *left, unsigned int value, unsigned int base)
{
    char tmp[16];
    const char *digits = "0123456789abcdef";
    int i = 0;
    do {
        tmp[i++] = digits[value % base];
        value /= base;
    } while (value);
    while (i--) out_char(buf, left, tmp[i]);
}

static void out_padded_uint(char **buf, size_t *left, unsigned int value,
                            unsigned int base, int width, int zero_pad)
{
    char tmp[16];
    const char *digits = "0123456789abcdef";
    int i = 0;
    do {
        tmp[i++] = digits[value % base];
        value /= base;
    } while (value);
    while (i < width) {
        tmp[i++] = zero_pad ? '0' : ' ';
    }
    while (i--) {
        out_char(buf, left, tmp[i]);
    }
}

static int vformat(char *buffer, size_t size, const char *fmt, va_list ap)
{
    char *cursor = buffer;
    size_t left = size;
    char **bufp = buffer ? &cursor : NULL;

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            out_char(bufp, &left, *fmt);
            continue;
        }
        fmt++;
        int zero_pad = 0;
        int width = 0;
        int precision = -1;

        if (*fmt == '0') {
            zero_pad = 1;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt++ - '0');
        }
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                precision = precision * 10 + (*fmt++ - '0');
            }
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z') {
            fmt++;
        }

        if (*fmt == 's') {
            out_str(bufp, &left, va_arg(ap, const char *));
        } else if (*fmt == 'd' || *fmt == 'i') {
            int value = va_arg(ap, int);
            if (value < 0) {
                out_char(bufp, &left, '-');
                value = -value;
            }
            int digits = precision >= 0 ? precision : width;
            out_padded_uint(bufp, &left, (unsigned int)value, 10, digits, zero_pad || precision >= 0);
        } else if (*fmt == 'u') {
            int digits = precision >= 0 ? precision : width;
            out_padded_uint(bufp, &left, va_arg(ap, unsigned int), 10, digits, zero_pad || precision >= 0);
        } else if (*fmt == 'x' || *fmt == 'p') {
            int digits = precision >= 0 ? precision : width;
            out_padded_uint(bufp, &left, va_arg(ap, unsigned int), 16, digits, zero_pad || precision >= 0);
        } else if (*fmt == 'f') {
            double value = va_arg(ap, double);
            if (value < 0) {
                out_char(bufp, &left, '-');
                value = -value;
            }
            unsigned int whole = (unsigned int)value;
            unsigned int scale = 1;
            int places = precision >= 0 ? precision : 3;
            for (int i = 0; i < places; i++) {
                scale *= 10;
            }
            unsigned int frac = (unsigned int)((value - whole) * scale + 0.5);
            out_uint(bufp, &left, whole, 10);
            if (places > 0) {
                out_char(bufp, &left, '.');
                out_padded_uint(bufp, &left, frac, 10, places, 1);
            }
        } else if (*fmt == 'c') {
            out_char(bufp, &left, va_arg(ap, int));
        } else if (*fmt == '%') {
            out_char(bufp, &left, '%');
        } else {
            out_char(bufp, &left, '%');
            out_char(bufp, &left, *fmt);
        }
    }
    if (buffer && size) {
        *cursor = 0;
    }
    return buffer ? (int)(cursor - buffer) : 0;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vformat(NULL, 0, fmt, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char *fmt, va_list ap)
{
    return vformat(NULL, 0, fmt, ap);
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    (void) stream;
    va_list ap;
    va_start(ap, fmt);
    int ret = vformat(NULL, 0, fmt, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    (void) stream;
    return vformat(NULL, 0, fmt, ap);
}

int sprintf(char *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vformat(str, (size_t)-1, fmt, ap);
    va_end(ap);
    return ret;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vformat(str, size, fmt, ap);
    va_end(ap);
    return ret;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    return vformat(str, size, fmt, ap);
}

int puts(const char *s)
{
    serial_write(s);
    serial_write("\n");
    return 0;
}

int putchar(int c)
{
    char s[2] = { (char)c, 0 };
    serial_write(s);
    return c;
}

FILE *fopen(const char *path, const char *mode)
{
    const unsigned char *data;
    size_t length;
    int write_mode = mode && (strchr(mode, 'w') || strchr(mode, 'a'));

    if (write_mode) {
        for (size_t i = 0; i < sizeof(ram_files) / sizeof(ram_files[0]); i++) {
            if (ram_files[i].kind == 0) {
                ram_files[i].kind = 3;
                ram_files[i].data = 0;
                ram_files[i].write_data = malloc(256);
                if (!ram_files[i].write_data) {
                    ram_files[i].kind = 0;
                    errno = ENOMEM;
                    return NULL;
                }
                ram_files[i].length = 0;
                ram_files[i].pos = 0;
                ram_files[i].capacity = 256;
                fs_normalize_path(path, ram_files[i].path, sizeof(ram_files[i].path));
                return &ram_files[i];
            }
        }
        errno = ENOMEM;
        return NULL;
    }

    if (fs_read_file(path, &data, &length) == 0) {
        for (size_t i = 0; i < sizeof(ram_files) / sizeof(ram_files[0]); i++) {
            if (ram_files[i].kind == 0) {
                ram_files[i].kind = 2;
                ram_files[i].data = data;
                ram_files[i].write_data = 0;
                ram_files[i].length = length;
                ram_files[i].pos = 0;
                ram_files[i].capacity = length;
                fs_normalize_path(path, ram_files[i].path, sizeof(ram_files[i].path));
                return &ram_files[i];
            }
        }
        errno = ENOMEM;
        return NULL;
    }
    errno = ENOENT;
    return NULL;
}

int fclose(FILE *stream)
{
    if (stream && stream->kind == 3) {
        fs_write_file(stream->path, stream->write_data, stream->length);
        free(stream->write_data);
    }
    if (stream && stream->kind == 2) {
        stream->kind = 0;
        stream->data = 0;
        stream->write_data = 0;
        stream->length = 0;
        stream->pos = 0;
        stream->capacity = 0;
    } else if (stream && stream->kind == 3) {
        stream->kind = 0;
        stream->data = 0;
        stream->write_data = 0;
        stream->length = 0;
        stream->pos = 0;
        stream->capacity = 0;
        stream->path[0] = 0;
    }
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (stream == NULL || stream->kind != 2 || size == 0) {
        return 0;
    }
    size_t bytes = size * nmemb;
    if (stream->pos + bytes > stream->length) {
        bytes = stream->length - stream->pos;
    }
    memcpy(ptr, stream->data + stream->pos, bytes);
    stream->pos += bytes;
    return bytes / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t bytes = size * nmemb;
    size_t needed;
    unsigned char *next;

    if (!stream || stream->kind != 3 || !ptr || size == 0) {
        return 0;
    }

    needed = stream->pos + bytes;
    if (needed > stream->capacity) {
        size_t next_capacity = stream->capacity ? stream->capacity : 256;
        while (next_capacity < needed) {
            next_capacity *= 2;
        }
        next = realloc(stream->write_data, next_capacity);
        if (!next) {
            return 0;
        }
        stream->write_data = next;
        stream->capacity = next_capacity;
    }

    memcpy(stream->write_data + stream->pos, ptr, bytes);
    stream->pos += bytes;
    if (stream->pos > stream->length) {
        stream->length = stream->pos;
    }
    return nmemb;
}

int fseek(FILE *stream, long offset, int whence)
{
    if (stream == NULL || (stream->kind != 2 && stream->kind != 3)) {
        return -1;
    }
    long base = 0;
    if (whence == SEEK_CUR) {
        base = (long) stream->pos;
    } else if (whence == SEEK_END) {
        base = (long) stream->length;
    }
    long next = base + offset;
    if (next < 0 || (size_t) next > stream->length) {
        return -1;
    }
    stream->pos = (size_t) next;
    return 0;
}

long ftell(FILE *stream)
{
    if (stream == NULL || (stream->kind != 2 && stream->kind != 3)) {
        return 0;
    }
    return (long) stream->pos;
}
int fflush(FILE *stream)
{
    if (stream && stream->kind == 3) {
        return fs_write_file(stream->path, stream->write_data, stream->length);
    }
    return 0;
}
int remove(const char *path) { return fs_delete(path); }
int rename(const char *oldpath, const char *newpath) { (void)oldpath; (void)newpath; return -1; }
int access(const char *path, int mode)
{
    (void) mode;
    if (fs_find(path)) {
        return 0;
    }
    errno = ENOENT;
    return -1;
}
int unlink(const char *path) { return remove(path); }
int mkdir(const char *path, unsigned int mode) { (void)mode; return fs_mkdir(path); }
int stat(const char *path, struct stat *buf)
{
    fs_stat_t st;

    if (fs_stat(path, &st) != 0) {
        errno = ENOENT;
        return -1;
    }

    if (buf != NULL) {
        buf->st_mode = st.type == FS_NODE_DIR ? S_IFDIR : S_IFREG;
        buf->st_size = (unsigned int)st.size;
    }
    return 0;
}

static int parse_int(const char *s, int base, int *out)
{
    int value = 0;
    int digits = 0;
    while (*s == ' ' || *s == '\t') s++;
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else break;
        if (d >= base) break;
        value = value * base + d;
        digits++;
        s++;
    }
    *out = value;
    return digits > 0;
}

int sscanf(const char *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int assigned = 0;
    while (*fmt) {
        if (*fmt++ != '%') continue;
        if (*fmt == 'x') {
            assigned += parse_int(str, 16, va_arg(ap, int *));
        } else if (*fmt == 'd' || *fmt == 'i') {
            assigned += parse_int(str, 10, va_arg(ap, int *));
        } else if (*fmt == 'f') {
            float *out = va_arg(ap, float *);
            *out = (float)atof(str);
            assigned++;
        }
        fmt++;
    }
    va_end(ap);
    return assigned;
}
