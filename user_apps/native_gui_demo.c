#include "tinyos.h"

static void draw_scene(tinyos_window_t win, uint32_t tick)
{
    uint32_t pulse = (tick / 80) % 96;
    uint32_t x = win.x + 42 + pulse * 3;
    uint32_t y = win.y + 146;

    tinyos_fill_rect(win.x + 24, win.y + 56, win.w - 48, win.h - 92, 0x00101720);
    tinyos_draw_rect(win.x + 24, win.y + 56, win.w - 48, win.h - 92, 0x0068a6bf);
    tinyos_draw_text_scaled(win.x + 38, win.y + 76, "C GUI .APP", 0x00fff08a, 2);
    tinyos_draw_text(win.x + 40, win.y + 122, "DRAWN BY A DISK-LOADED C PROGRAM", 0x00d7e4ea);
    tinyos_fill_rect(x, y, 54, 28, 0x0000bcd4);
    tinyos_draw_rect(x, y, 54, 28, 0x00ffffff);
    tinyos_draw_text(x + 12, y + 10, "APP", 0x00080c10);
    tinyos_draw_footer("ESC OR ENTER RETURNS TO COMMAND");
}

int tinyos_main(tinyos_api_t *api)
{
    tinyos_window_t win;
    uint32_t last_tick = 0;

    (void)api;

    win = tinyos_draw_window(560, 300, "CGUI.APP");
    tinyos_write("CGUI.APP USING LIBTINYOS GUI API");
    draw_scene(win, tinyos_ticks_ms());

    for (;;) {
        uint8_t key;
        uint32_t tick = tinyos_ticks_ms();

        if (tinyos_poll_key(&key) && (key == TINYOS_KEY_ESCAPE || key == TINYOS_KEY_ENTER || key == ' ')) {
            tinyos_write("CGUI.APP RETURNING TO LOADER");
            return 0;
        }

        if (tick - last_tick >= 80) {
            draw_scene(win, tick);
            last_tick = tick;
        }
        tinyos_sleep(10);
    }
}
