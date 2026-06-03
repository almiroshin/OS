#include <stdio.h>
#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "timer.h"

static void draw_clock(void)
{
    char line[64];
    uint32_t seconds = timer_ticks_ms() / 1000;
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds / 60) % 60;
    uint32_t secs = seconds % 60;
    app_window_t win = app_draw_window(456, 246, "CLOCK");

    snprintf(line, sizeof(line), "%02u:%02u:%02u", hours, minutes, secs);
    framebuffer_draw_text_scaled(win.x + 92, win.y + 78, line, 0x00fff08a, 3);

    snprintf(line, sizeof(line), "UPTIME %u SECONDS", seconds);
    framebuffer_draw_text(win.x + 118, win.y + 154, line, 0x00d7e4ea);
    framebuffer_draw_text(win.x + 118, win.y + 182, "PIT TIMER BASED", 0x00d7e4ea);
    framebuffer_draw_text(win.x + 118, win.y + 210, "ESC RETURN", 0x00a7f3ff);
}

app_result_t app_clock_main(void)
{
    uint32_t last_draw = 0xffffffffu;

    for (;;) {
        unsigned char key;
        uint32_t now = timer_ticks_ms();

        while (app_read_key(&key)) {
            if (key == KEY_ESCAPE || key == KEY_ENTER || key == ' ') {
                return APP_RESULT_EXIT;
            }
        }

        if (now / 250 != last_draw / 250) {
            draw_clock();
            last_draw = now;
        }

        timer_sleep_ms(10);
    }
}
