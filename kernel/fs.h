#pragma once
#include <stddef.h>
#include <stdint.h>

#define FS_ATTR_READONLY 0x01u
#define FS_ATTR_DIRECTORY 0x10u
#define FS_ATTR_ARCHIVE 0x20u

typedef enum {
    FS_NODE_FILE = 1,
    FS_NODE_DIR = 2,
} fs_node_type_t;

typedef struct {
    char path[64];
    fs_node_type_t type;
    const unsigned char *data;
    size_t size;
    size_t capacity;
    uint32_t attr;
    int dynamic_data;
} fs_node_t;

typedef struct {
    char path[64];
    fs_node_type_t type;
    size_t size;
    uint32_t attr;
} fs_stat_t;

typedef void (*fs_list_callback_t)(const fs_node_t *node, void *ctx);

void fs_init(void);
int fs_normalize_path(const char *path, char *out, size_t out_size);
const fs_node_t *fs_find(const char *path);
int fs_stat(const char *path, fs_stat_t *out);
int fs_read_file(const char *path, const unsigned char **data, size_t *size);
int fs_write_file(const char *path, const void *data, size_t size);
int fs_append_file(const char *path, const void *data, size_t size);
int fs_delete(const char *path);
int fs_mkdir(const char *path);
int fs_list(const char *path, fs_list_callback_t callback, void *ctx);
int fs_node_count(void);
int fs_file_count(void);
int fs_free_node_count(void);
int fs_is_persistent(void);
uint32_t fs_persist_generation(void);
int fs_flush(void);
