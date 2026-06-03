#include "app_ui.h"
#include "framebuffer.h"
#include "keyboard.h"

uint32_t app_center_coord(uint32_t outer, uint32_t inner)
{
    return outer > inner ? (outer - inner) / 2 : 0;
}

app_window_t app_draw_window(uint32_t w, uint32_t h, const char *title)
{
    app_window_t win;

    if (w > framebuffer_width()) {
        w = framebuffer_width();
    }
    if (h > framebuffer_height()) {
        h = framebuffer_height();
    }

    win.x = app_center_coord(framebuffer_width(), w);
    win.y = app_center_coord(framebuffer_height(), h);
    win.w = w;
    win.h = h;

    framebuffer_clear(0x00101418);
    framebuffer_fill_rect(win.x + 6, win.y + 6, win.w, win.h, 0x00080c10);
    framebuffer_fill_rect(win.x, win.y, win.w, win.h, 0x001a222c);
    framebuffer_draw_rect(win.x, win.y, win.w, win.h, 0x0068a6bf);
    framebuffer_fill_rect(win.x, win.y, win.w, 30, 0x00313c4d);
    framebuffer_draw_text(win.x + 14, win.y + 10, title, 0x00ffffff);

    return win;
}

void app_draw_footer(const char *text)
{
    uint32_t h = framebuffer_height();

    if (h < 28) {
        return;
    }

    framebuffer_fill_rect(16, h - 26, framebuffer_width() - 32, 1, 0x00344552);
    framebuffer_draw_text(20, h - 18, text, 0x00b9c8d0);
}

int app_read_key(unsigned char *key)
{
    int pressed;

    while (keyboard_get_doom_key(&pressed, key)) {
        if (pressed) {
            return 1;
        }
    }

    return 0;
}
