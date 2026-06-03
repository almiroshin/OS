#include <stdio.h>
#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "timer.h"

static int acc;
static int current;
static int op;
static int entering;
static int error;

static void calc_reset(void)
{
    acc = 0;
    current = 0;
    op = 0;
    entering = 0;
    error = 0;
}

static void calc_apply(void)
{
    if (error) {
        return;
    }

    if (op == 0) {
        acc = current;
    } else if (op == '+') {
        acc += current;
    } else if (op == '-') {
        acc -= current;
    } else if (op == '*') {
        acc *= current;
    } else if (op == '/') {
        if (current == 0) {
            error = 1;
        } else {
            acc /= current;
        }
    }

    current = acc;
    entering = 0;
}

static void calc_digit(int digit)
{
    if (error) {
        calc_reset();
    }
    if (!entering) {
        current = 0;
        entering = 1;
    }
    if (current < 100000000) {
        current = current * 10 + digit;
    }
}

static void calc_operator(int next_op)
{
    if (entering || op == 0) {
        calc_apply();
    }
    op = next_op;
    entering = 0;
    current = acc;
}

static int normalize_operator(unsigned char key)
{
    if (key == '+' || key == 'a') {
        return '+';
    }
    if (key == '-' || key == 's') {
        return '-';
    }
    if (key == '*' || key == 'm') {
        return '*';
    }
    if (key == '/' || key == 'd') {
        return '/';
    }
    return 0;
}

static void draw_calc(void)
{
    char line[64];
    app_window_t win = app_draw_window(456, 292, "CALCULATOR");

    framebuffer_fill_rect(win.x + 30, win.y + 54, win.w - 60, 58, 0x00091014);
    framebuffer_draw_rect(win.x + 30, win.y + 54, win.w - 60, 58, 0x004a6575);

    if (error) {
        framebuffer_draw_text_scaled(win.x + 50, win.y + 72, "DIV ZERO", 0x00ff6b6b, 2);
    } else {
        snprintf(line, sizeof(line), "%d", entering ? current : acc);
        framebuffer_draw_text_scaled(win.x + 50, win.y + 72, line, 0x00fff08a, 2);
    }

    snprintf(line, sizeof(line), "ACC %d    OP %c", acc, op ? op : '-');
    framebuffer_draw_text(win.x + 34, win.y + 132, line, 0x00d7e4ea);
    framebuffer_draw_text(win.x + 34, win.y + 162, "0-9 TYPE NUMBER", 0x00d7e4ea);
    framebuffer_draw_text(win.x + 34, win.y + 186, "A ADD   S SUB   M MUL   D DIV", 0x00d7e4ea);
    framebuffer_draw_text(win.x + 34, win.y + 210, "ENTER EQUAL   C CLEAR", 0x00d7e4ea);
    framebuffer_draw_text(win.x + 34, win.y + 246, "ESC RETURN", 0x00a7f3ff);
}

app_result_t app_calc_main(void)
{
    calc_reset();
    draw_calc();

    for (;;) {
        unsigned char key;

        while (app_read_key(&key)) {
            int next_op;

            if (key == KEY_ESCAPE) {
                return APP_RESULT_EXIT;
            }
            if (key >= '0' && key <= '9') {
                calc_digit(key - '0');
            } else if (key == KEY_BACKSPACE) {
                current /= 10;
            } else if (key == 'c') {
                calc_reset();
            } else if (key == KEY_ENTER || key == '=') {
                calc_apply();
                op = 0;
            } else {
                next_op = normalize_operator(key);
                if (next_op != 0) {
                    calc_operator(next_op);
                }
            }

            draw_calc();
        }

        timer_sleep_ms(10);
    }
}
