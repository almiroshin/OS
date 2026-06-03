#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "timer.h"

#define PAINT_W 32
#define PAINT_H 18
#define PAINT_CELL 12

static unsigned char canvas[PAINT_W * PAINT_H];
static int cursor_x;
static int cursor_y;
static unsigned char brush = 1;

static const uint32_t colors[] = {
    0x000f151b,
    0x00fff08a,
    0x00a7f3ff,
    0x00ff6b6b,
    0x0086e389,
};

static void clear_canvas(void)
{
    for (int i = 0; i < PAINT_W * PAINT_H; i++) {
        canvas[i] = 0;
    }
}

static void draw_paint(void)
{
    app_window_t win = app_draw_window(520, 360, "PAINT");
    uint32_t grid_x = win.x + 28;
    uint32_t grid_y = win.y + 56;

    framebuffer_fill_rect(grid_x - 1, grid_y - 1,
                          PAINT_W * PAINT_CELL + 2,
                          PAINT_H * PAINT_CELL + 2,
                          0x004a6575);

    for (int y = 0; y < PAINT_H; y++) {
        for (int x = 0; x < PAINT_W; x++) {
            uint32_t px = grid_x + (uint32_t)x * PAINT_CELL;
            uint32_t py = grid_y + (uint32_t)y * PAINT_CELL;
            unsigned char color = canvas[y * PAINT_W + x];

            if (color >= sizeof(colors) / sizeof(colors[0])) {
                color = 0;
            }

            framebuffer_fill_rect(px, py, PAINT_CELL - 1, PAINT_CELL - 1, colors[color]);
        }
    }

    framebuffer_draw_rect(grid_x + (uint32_t)cursor_x * PAINT_CELL,
                          grid_y + (uint32_t)cursor_y * PAINT_CELL,
                          PAINT_CELL, PAINT_CELL, 0x00ffffff);
    framebuffer_draw_rect(grid_x + (uint32_t)cursor_x * PAINT_CELL + 1,
                          grid_y + (uint32_t)cursor_y * PAINT_CELL + 1,
                          PAINT_CELL - 2, PAINT_CELL - 2, 0x00fff08a);

    framebuffer_draw_text(win.x + 28, win.y + 282, "ARROWS MOVE   SPACE DRAW   BACKSPACE ERASE", 0x00d7e4ea);
    framebuffer_draw_text(win.x + 28, win.y + 306, "1-4 COLOR   C CLEAR   ESC RETURN", 0x00d7e4ea);
    framebuffer_fill_rect(win.x + 408, win.y + 298, 28, 18, colors[brush]);
    framebuffer_draw_rect(win.x + 408, win.y + 298, 28, 18, 0x00ffffff);
}

app_result_t app_paint_main(void)
{
    draw_paint();

    for (;;) {
        unsigned char key;

        while (app_read_key(&key)) {
            if (key == KEY_ESCAPE || key == KEY_ENTER) {
                return APP_RESULT_EXIT;
            }
            if (key == KEY_LEFTARROW && cursor_x > 0) {
                cursor_x--;
            } else if (key == KEY_RIGHTARROW && cursor_x < PAINT_W - 1) {
                cursor_x++;
            } else if (key == KEY_UPARROW && cursor_y > 0) {
                cursor_y--;
            } else if (key == KEY_DOWNARROW && cursor_y < PAINT_H - 1) {
                cursor_y++;
            } else if (key == ' ') {
                canvas[cursor_y * PAINT_W + cursor_x] = brush;
            } else if (key == KEY_BACKSPACE || key == '0') {
                canvas[cursor_y * PAINT_W + cursor_x] = 0;
            } else if (key >= '1' && key <= '4') {
                brush = key - '0';
            } else if (key == 'c') {
                clear_canvas();
            }

            draw_paint();
        }

        timer_sleep_ms(10);
    }
}
