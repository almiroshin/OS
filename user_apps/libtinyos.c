#include "tinyos.h"

static tinyos_api_t *bound_api = 0;

void tinyos_bind(tinyos_api_t *api)
{
    bound_api = api;
}

tinyos_api_t *tinyos_get_api(void)
{
    return bound_api;
}

void tinyos_write(const char *text)
{
    if (bound_api && bound_api->write) {
        bound_api->write(text);
    }
}

void tinyos_mem_info(void)
{
    if (bound_api && bound_api->mem_info) {
        bound_api->mem_info();
    }
}

void tinyos_proc_info(void)
{
    if (bound_api && bound_api->proc_info) {
        bound_api->proc_info();
    }
}

void tinyos_sleep(uint32_t ms)
{
    if (bound_api && bound_api->sleep) {
        bound_api->sleep(ms);
    }
}

int tinyos_list_dir(const char *path)
{
    if (!bound_api || !bound_api->list_dir) {
        return -1;
    }
    return bound_api->list_dir(path);
}

void tinyos_exit(int code)
{
    if (bound_api && bound_api->exit) {
        bound_api->exit(code);
    }
}

uint32_t tinyos_ticks_ms(void)
{
    if (!bound_api || !bound_api->ticks_ms) {
        return 0;
    }
    return bound_api->ticks_ms();
}

tinyos_window_t tinyos_draw_window(uint32_t w, uint32_t h, const char *title)
{
    tinyos_window_t empty = { 0, 0, 0, 0 };

    if (!bound_api || !bound_api->draw_window) {
        return empty;
    }
    return bound_api->draw_window(w, h, title);
}

void tinyos_draw_footer(const char *text)
{
    if (bound_api && bound_api->draw_footer) {
        bound_api->draw_footer(text);
    }
}

void tinyos_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (bound_api && bound_api->fill_rect) {
        bound_api->fill_rect(x, y, w, h, color);
    }
}

void tinyos_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (bound_api && bound_api->draw_rect) {
        bound_api->draw_rect(x, y, w, h, color);
    }
}

void tinyos_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t color)
{
    if (bound_api && bound_api->draw_text) {
        bound_api->draw_text(x, y, text, color);
    }
}

void tinyos_draw_text_scaled(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t scale)
{
    if (bound_api && bound_api->draw_text_scaled) {
        bound_api->draw_text_scaled(x, y, text, color, scale);
    }
}

void tinyos_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (bound_api && bound_api->put_pixel) {
        bound_api->put_pixel(x, y, color);
    }
}

int tinyos_poll_key(uint8_t *key)
{
    if (!bound_api || !bound_api->poll_key) {
        return 0;
    }
    return bound_api->poll_key(key);
}
