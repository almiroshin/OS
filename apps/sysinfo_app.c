#include <stdio.h>
#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "fs.h"
#include "memory.h"
#include "process.h"
#include "timer.h"

extern unsigned int doom_iwad_len;

static void draw_sysinfo(void)
{
    char line[80];
    uint32_t seconds = timer_ticks_ms() / 1000;
    memory_stats_t mem;
    app_window_t win = app_draw_window(560, 360, "SYSTEM INFO");

    memory_get_stats(&mem);

    framebuffer_draw_text_scaled(win.x + 42, win.y + 50, "TINYDOOMOS", 0x00fff08a, 2);
    framebuffer_draw_text(win.x + 44, win.y + 92, "32 BIT PET PROJECT OS", 0x00d7e4ea);

    snprintf(line, sizeof(line), "FRAMEBUFFER %u X %u", framebuffer_width(), framebuffer_height());
    framebuffer_draw_text(win.x + 44, win.y + 122, line, 0x00d7e4ea);

    snprintf(line, sizeof(line), "APPS %u  C: FILES %u", (unsigned int)app_count(), (unsigned int)fs_file_count());
    framebuffer_draw_text(win.x + 44, win.y + 146, line, 0x00d7e4ea);

    snprintf(line, sizeof(line), "TINYFS %s GEN %u",
             fs_is_persistent() ? "DISK" : "RAM",
             (unsigned int)fs_persist_generation());
    framebuffer_draw_text(win.x + 44, win.y + 170, line, 0x00d7e4ea);

    snprintf(line, sizeof(line), "UPTIME %u SECONDS", seconds);
    framebuffer_draw_text(win.x + 44, win.y + 194, line, 0x00d7e4ea);

    snprintf(line, sizeof(line), "EMBEDDED WAD %u KB", doom_iwad_len / 1024);
    framebuffer_draw_text(win.x + 44, win.y + 218, line, 0x00d7e4ea);

    snprintf(line, sizeof(line), "HEAP USED %u KB  FREE %u KB", mem.heap_used / 1024, mem.heap_free / 1024);
    framebuffer_draw_text(win.x + 44, win.y + 242, line, 0x00d7e4ea);

    snprintf(line, sizeof(line), "PROCESSES ACTIVE %u  STARTED %u",
             (unsigned int)process_active_count(),
             (unsigned int)process_total_started());
    framebuffer_draw_text(win.x + 44, win.y + 266, line, 0x00d7e4ea);

    framebuffer_draw_text(win.x + 44, win.y + 296, "LAYERS TINYFS EXEC SYSCALL PROCESS HEAP", 0x00d7e4ea);
    framebuffer_draw_text(win.x + 44, win.y + 316, "ESC RETURN", 0x00a7f3ff);
}

app_result_t app_sysinfo_main(void)
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

        if (now / 500 != last_draw / 500) {
            draw_sysinfo();
            last_draw = now;
        }

        timer_sleep_ms(10);
    }
}
