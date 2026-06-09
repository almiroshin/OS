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
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} tinyos_window_t;

typedef struct {
    void (*write)(const char *text);
    void (*mem_info)(void);
    void (*proc_info)(void);
    void (*sleep)(uint32_t ms);
    int (*list_dir)(const char *path);
    void (*exit)(int code);
    uint32_t (*ticks_ms)(void);
    tinyos_window_t (*draw_window)(uint32_t w, uint32_t h, const char *title);
    void (*draw_footer)(const char *text);
    void (*fill_rect)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
    void (*draw_rect)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
    void (*draw_text)(uint32_t x, uint32_t y, const char *text, uint32_t color);
    void (*draw_text_scaled)(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t scale);
    void (*put_pixel)(uint32_t x, uint32_t y, uint32_t color);
    int (*poll_key)(uint8_t *key);
} tinyos_api_t;

typedef int (*tinyos_app_entry_t)(tinyos_api_t *api);

int tinyos_main(tinyos_api_t *api);

#endif

#endif
