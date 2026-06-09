#include "tinyos_app.h"

#define KEY_ESCAPE 27
#define KEY_ENTER 13

static void draw_scene(tinyos_api_t *api, tinyos_window_t win, uint32_t tick)
{
    uint32_t pulse = (tick / 80) % 96;
    uint32_t x = win.x + 42 + pulse * 3;
    uint32_t y = win.y + 146;

    api->fill_rect(win.x + 24, win.y + 56, win.w - 48, win.h - 92, 0x00101720);
    api->draw_rect(win.x + 24, win.y + 56, win.w - 48, win.h - 92, 0x0068a6bf);
    api->draw_text_scaled(win.x + 38, win.y + 76, "C GUI .APP", 0x00fff08a, 2);
    api->draw_text(win.x + 40, win.y + 122, "DRAWN BY A DISK-LOADED C PROGRAM", 0x00d7e4ea);
    api->fill_rect(x, y, 54, 28, 0x0000bcd4);
    api->draw_rect(x, y, 54, 28, 0x00ffffff);
    api->draw_text(x + 12, y + 10, "APP", 0x00080c10);
    api->draw_footer("ESC OR ENTER RETURNS TO COMMAND");
}

int tinyos_main(tinyos_api_t *api)
{
    tinyos_window_t win = api->draw_window(560, 300, "CGUI.APP");
    uint32_t last_tick = 0;

    api->write("CGUI.APP USING NATIVE GUI API");
    draw_scene(api, win, api->ticks_ms());

    for (;;) {
        uint8_t key;
        uint32_t tick = api->ticks_ms();

        if (api->poll_key(&key) && (key == KEY_ESCAPE || key == KEY_ENTER || key == ' ')) {
            api->write("CGUI.APP RETURNING TO LOADER");
            return 0;
        }

        if (tick - last_tick >= 80) {
            draw_scene(api, win, tick);
            last_tick = tick;
        }
        api->sleep(10);
    }
}
