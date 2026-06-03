#pragma once
#include <stdint.h>
#include "multiboot.h"
void framebuffer_init(const multiboot_info_t *mbi);
void framebuffer_clear(uint32_t color);
void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t framebuffer_width(void);
uint32_t framebuffer_height(void);
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void framebuffer_draw_text(uint32_t x, uint32_t y, const char *s, uint32_t color);
void framebuffer_draw_text_scaled(uint32_t x, uint32_t y, const char *s, uint32_t color, uint32_t scale);
void framebuffer_blit_doom(const uint32_t *pixels, uint32_t w, uint32_t h);
void framebuffer_write_text(const char *s);
