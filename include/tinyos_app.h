#ifndef TINYOS_APP_H
#define TINYOS_APP_H

#define TINYOS_APP_MAGIC 0x31505041

#ifndef __ASSEMBLER__

#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t entry_offset;
    uint32_t image_size;
    uint32_t flags;
} tinyos_app_header_t;

typedef struct {
    void (*write)(const char *text);
    void (*mem_info)(void);
    void (*proc_info)(void);
    void (*sleep)(uint32_t ms);
    int (*list_dir)(const char *path);
    void (*exit)(int code);
} tinyos_api_t;

typedef int (*tinyos_app_entry_t)(tinyos_api_t *api);

int tinyos_main(tinyos_api_t *api);

#endif

#endif
