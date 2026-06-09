#ifndef TINYOS_H
#define TINYOS_H

#include "tinyos_app.h"

#define TINYOS_KEY_ESCAPE 27
#define TINYOS_KEY_ENTER 13

void tinyos_bind(tinyos_api_t *api);
tinyos_api_t *tinyos_get_api(void);

void tinyos_write(const char *text);
void tinyos_mem_info(void);
void tinyos_proc_info(void);
void tinyos_sleep(uint32_t ms);
int tinyos_list_dir(const char *path);
void tinyos_exit(int code);
uint32_t tinyos_ticks_ms(void);

tinyos_window_t tinyos_draw_window(uint32_t w, uint32_t h, const char *title);
void tinyos_draw_footer(const char *text);
void tinyos_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void tinyos_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void tinyos_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t color);
void tinyos_draw_text_scaled(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t scale);
void tinyos_put_pixel(uint32_t x, uint32_t y, uint32_t color);
int tinyos_poll_key(uint8_t *key);

#endif
