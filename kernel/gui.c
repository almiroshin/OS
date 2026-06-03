#include "doomkeys.h"
#include "app.h"
#include "framebuffer.h"
#include "gui.h"
#include "keyboard.h"
#include "serial.h"
#include "timer.h"

#define GUI_HALT 0
#define GUI_CONTINUE 1

static int selected;
static int list_top;

static uint32_t center_coord(uint32_t outer, uint32_t inner)
{
    return outer > inner ? (outer - inner) / 2 : 0;
}

static void draw_button(uint32_t x, uint32_t y, uint32_t w, const char *label, int active)
{
    uint32_t fill = active ? 0x002a6f92 : 0x00132332;
    uint32_t border = active ? 0x00a7f3ff : 0x00455968;
    uint32_t text = active ? 0x00ffffff : 0x00d7e4ea;

    framebuffer_fill_rect(x, y, w, 34, fill);
    framebuffer_draw_rect(x, y, w, 34, border);
    if (active) {
        framebuffer_draw_text(x + 14, y + 10, ">", 0x00fff08a);
    }
    framebuffer_draw_text(x + 34, y + 10, label, text);
}

static void draw_background(void)
{
    uint32_t h = framebuffer_height();
    for (uint32_t y = 0; y < h; y++) {
        uint32_t band = y * 60 / (h ? h : 1);
        uint32_t color = 0x0011161d + (band << 8);
        framebuffer_fill_rect(0, y, framebuffer_width(), 1, color);
    }

    framebuffer_fill_rect(0, 0, framebuffer_width(), 28, 0x00272f3d);
    framebuffer_fill_rect(0, 28, framebuffer_width(), 1, 0x0068a6bf);
    framebuffer_draw_text(12, 9, "TINYDOOMOS", 0x00ffffff);
    framebuffer_draw_text(framebuffer_width() - 128, 9, "GUI 0.1", 0x00a7f3ff);

    framebuffer_fill_rect(16, framebuffer_height() - 26, framebuffer_width() - 32, 1, 0x00344552);
    framebuffer_draw_text(20, framebuffer_height() - 18, "ARROWS SELECT   ENTER OK   ESC HALT", 0x00b9c8d0);
}

static void draw_app_panel(uint32_t x, uint32_t y)
{
    const app_descriptor_t *app = app_get(selected);
    framebuffer_fill_rect(x, y, 190, 146, 0x0019232c);
    framebuffer_draw_rect(x, y, 190, 146, 0x004a6575);
    framebuffer_draw_text(x + 14, y + 14, "APP INFO", 0x00fff08a);
    if (app != 0) {
        framebuffer_draw_text(x + 14, y + 40, app->title, 0x00ffffff);
        framebuffer_draw_text(x + 14, y + 68, app->summary, 0x00d7e4ea);
        framebuffer_draw_text(x + 14, y + 96, "ENTER TO RUN", 0x00a7f3ff);
    } else {
        framebuffer_draw_text(x + 14, y + 40, "HALT", 0x00ffffff);
        framebuffer_draw_text(x + 14, y + 68, "STOP THE SYSTEM", 0x00d7e4ea);
        framebuffer_draw_text(x + 14, y + 96, "ENTER TO HALT", 0x00a7f3ff);
    }
}

static uint32_t launcher_height(uint32_t *visible_slots)
{
    uint32_t total_items = (uint32_t)app_count() + 1;
    uint32_t desired = 154 + total_items * 42;
    uint32_t max_h = framebuffer_height() > 78 ? framebuffer_height() - 78 : framebuffer_height();
    uint32_t win_h = desired;

    if (win_h > max_h) {
        win_h = max_h;
    }
    if (win_h < 220) {
        win_h = 220;
    }

    *visible_slots = win_h > 154 ? (win_h - 154) / 42 : 1;
    if (*visible_slots < 1) {
        *visible_slots = 1;
    }
    if (*visible_slots > total_items) {
        *visible_slots = total_items;
    }

    win_h = 154 + *visible_slots * 42;
    if (win_h < 276 && desired >= 276 && max_h >= 276) {
        win_h = 276;
    }

    return win_h;
}

static void keep_selection_visible(int visible_slots)
{
    int total_items = app_count() + 1;

    if (selected < list_top) {
        list_top = selected;
    }
    if (selected >= list_top + visible_slots) {
        list_top = selected - visible_slots + 1;
    }
    if (list_top < 0) {
        list_top = 0;
    }
    if (list_top > total_items - visible_slots) {
        list_top = total_items - visible_slots;
    }
    if (list_top < 0) {
        list_top = 0;
    }
}

static void draw_desktop(void)
{
    uint32_t visible_slots;
    uint32_t win_h = launcher_height(&visible_slots);
    uint32_t win_x = center_coord(framebuffer_width(), 492);
    uint32_t win_y = center_coord(framebuffer_height(), win_h);
    uint32_t win_w = 492;

    keep_selection_visible((int)visible_slots);

    if (win_y < 44) {
        win_y = 44;
    }
    if (win_y + win_h + 34 > framebuffer_height() && framebuffer_height() > win_h + 34) {
        win_y = framebuffer_height() - win_h - 34;
    }

    draw_background();

    framebuffer_fill_rect(win_x + 6, win_y + 6, win_w, win_h, 0x00080c10);
    framebuffer_fill_rect(win_x, win_y, win_w, win_h, 0x00172029);
    framebuffer_draw_rect(win_x, win_y, win_w, win_h, 0x0068a6bf);
    framebuffer_fill_rect(win_x, win_y, win_w, 30, 0x00313c4d);
    framebuffer_draw_text(win_x + 14, win_y + 10, "DESKTOP LAUNCHER", 0x00ffffff);

    framebuffer_draw_text_scaled(win_x + 34, win_y + 52, "TINYDOOMOS", 0x00fff08a, 2);
    framebuffer_draw_text(win_x + 36, win_y + 86, "A SMALL GUI BEFORE BIG DOOM", 0x00b9c8d0);

    for (uint32_t slot = 0; slot < visible_slots; slot++) {
        int index = list_top + (int)slot;
        const char *title = "HALT";

        if (index < app_count()) {
            title = app_get(index)->title;
        }

        draw_button(win_x + 34, win_y + 118 + slot * 42, 220, title, selected == index);
    }

    if (list_top > 0) {
        framebuffer_draw_text(win_x + 242, win_y + 124, "...", 0x00fff08a);
    }
    if (list_top + (int)visible_slots < app_count() + 1) {
        framebuffer_draw_text(win_x + 242, win_y + 118 + visible_slots * 42 - 18, "...", 0x00fff08a);
    }

    draw_app_panel(win_x + 280, win_y + 104);
}

int gui_run(void)
{
    serial_write("TinyDoomOS: GUI start\n");
    selected = 0;
    list_top = 0;
    draw_desktop();

    for (;;) {
        int pressed;
        unsigned char key;

        while (keyboard_get_doom_key(&pressed, &key)) {
            if (!pressed) {
                continue;
            }

            if (key == KEY_UPARROW) {
                selected = (selected + app_count()) % (app_count() + 1);
                draw_desktop();
            } else if (key == KEY_DOWNARROW) {
                selected = (selected + 1) % (app_count() + 1);
                draw_desktop();
            } else if (key == KEY_ENTER || key == KEY_FIRE || key == ' ') {
                if (selected < app_count()) {
                    const app_descriptor_t *app = app_get(selected);
                    serial_write("TinyDoomOS: GUI launch app\n");
                    if (app_run(app) == APP_RESULT_HALT) {
                        return GUI_HALT;
                    }
                    draw_desktop();
                } else {
                    serial_write("TinyDoomOS: GUI halt\n");
                    return GUI_HALT;
                }
            } else if (key == KEY_ESCAPE) {
                return GUI_HALT;
            }
        }

        timer_sleep_ms(10);
    }

    return GUI_CONTINUE;
}
