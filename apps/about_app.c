#include "app.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "timer.h"

static uint32_t center_coord(uint32_t outer, uint32_t inner)
{
    return outer > inner ? (outer - inner) / 2 : 0;
}

static void draw_about(void)
{
    uint32_t win_w = 512;
    uint32_t win_h = 288;
    uint32_t x = center_coord(framebuffer_width(), win_w);
    uint32_t y = center_coord(framebuffer_height(), win_h);

    framebuffer_clear(0x00101418);
    framebuffer_fill_rect(x + 6, y + 6, win_w, win_h, 0x00080c10);
    framebuffer_fill_rect(x, y, win_w, win_h, 0x001a222c);
    framebuffer_draw_rect(x, y, win_w, win_h, 0x0068a6bf);
    framebuffer_fill_rect(x, y, win_w, 30, 0x00313c4d);
    framebuffer_draw_text(x + 14, y + 10, "ABOUT TINYDOOMOS", 0x00ffffff);

    framebuffer_draw_text_scaled(x + 32, y + 56, "TINYDOOMOS", 0x00fff08a, 2);
    framebuffer_draw_text(x + 34, y + 94, "SMALL 32 BIT PET PROJECT OS", 0x00d7e4ea);
    framebuffer_draw_text(x + 34, y + 118, "STYLE: WINDOWS 3.X INSPIRED", 0x00d7e4ea);
    framebuffer_draw_text(x + 34, y + 142, "APPS: C MODULES TAP AND APP", 0x00d7e4ea);
    framebuffer_draw_text(x + 34, y + 166, "OS: FS DISK EXEC SYSCALL HEAP", 0x00d7e4ea);
    framebuffer_draw_text(x + 34, y + 190, "NEXT: PAGING USER MODE MOUSE", 0x00d7e4ea);
    framebuffer_draw_text(x + 34, y + 226, "ENTER OR ESC TO RETURN", 0x00a7f3ff);
}

app_result_t app_about_main(void)
{
    draw_about();

    for (;;) {
        int pressed;
        unsigned char key;
        while (keyboard_get_doom_key(&pressed, &key)) {
            if (!pressed) {
                continue;
            }
            if (key == KEY_ENTER || key == KEY_ESCAPE || key == ' ') {
                return APP_RESULT_EXIT;
            }
        }
        timer_sleep_ms(10);
    }
}
