#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "timer.h"

#define NOTE_MAX 512
#define NOTE_LINES 18
#define NOTE_COLS 62

static char note[NOTE_MAX];
static int note_len;

static char printable_key(unsigned char key)
{
    if (key >= 'a' && key <= 'z') {
        return (char)(key - 'a' + 'A');
    }
    if ((key >= '0' && key <= '9') || key == ' ' || key == '.' ||
        key == ',' || key == '-' || key == '/' || key == ':' ||
        key == '?' || key == '!' || key == '+') {
        return (char)key;
    }
    return 0;
}

static void draw_notepad(void)
{
    char lines[NOTE_LINES][NOTE_COLS + 1];
    int line = 0;
    int col = 0;
    int cursor_line = 0;
    int cursor_col = 0;
    app_window_t win = app_draw_window(600, 384, "NOTEPAD");

    for (int y = 0; y < NOTE_LINES; y++) {
        for (int x = 0; x <= NOTE_COLS; x++) {
            lines[y][x] = 0;
        }
    }

    for (int i = 0; i < note_len && line < NOTE_LINES; i++) {
        char c = note[i];

        if (c == '\n') {
            line++;
            col = 0;
            continue;
        }

        if (col >= NOTE_COLS) {
            line++;
            col = 0;
            if (line >= NOTE_LINES) {
                break;
            }
        }

        lines[line][col++] = c;
    }

    cursor_line = line;
    cursor_col = col;

    framebuffer_fill_rect(win.x + 22, win.y + 48, win.w - 44, win.h - 104, 0x000f151b);
    framebuffer_draw_rect(win.x + 22, win.y + 48, win.w - 44, win.h - 104, 0x004a6575);

    for (int y = 0; y < NOTE_LINES; y++) {
        framebuffer_draw_text(win.x + 34, win.y + 60 + (uint32_t)y * 12, lines[y], 0x00d7e4ea);
    }

    if (cursor_line < NOTE_LINES && cursor_col < NOTE_COLS) {
        framebuffer_draw_text(win.x + 34 + (uint32_t)cursor_col * 8,
                              win.y + 60 + (uint32_t)cursor_line * 12,
                              "_", 0x00fff08a);
    }

    framebuffer_draw_text(win.x + 28, win.y + win.h - 38,
                          "TYPE TEXT   BACKSPACE DELETE   ENTER NEW LINE   ESC RETURN",
                          0x00a7f3ff);
}

app_result_t app_notepad_main(void)
{
    draw_notepad();

    for (;;) {
        unsigned char key;

        while (app_read_key(&key)) {
            char c;

            if (key == KEY_ESCAPE) {
                return APP_RESULT_EXIT;
            }
            if (key == KEY_BACKSPACE) {
                if (note_len > 0) {
                    note[--note_len] = 0;
                }
            } else if (key == KEY_ENTER) {
                if (note_len < NOTE_MAX - 1) {
                    note[note_len++] = '\n';
                    note[note_len] = 0;
                }
            } else {
                c = printable_key(key);
                if (c != 0 && note_len < NOTE_MAX - 1) {
                    note[note_len++] = c;
                    note[note_len] = 0;
                }
            }

            draw_notepad();
        }

        timer_sleep_ms(10);
    }
}
