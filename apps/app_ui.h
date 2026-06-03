#pragma once
#include <stdint.h>

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} app_window_t;

uint32_t app_center_coord(uint32_t outer, uint32_t inner);
app_window_t app_draw_window(uint32_t w, uint32_t h, const char *title);
void app_draw_footer(const char *text);
int app_read_key(unsigned char *key);
