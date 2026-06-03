#include "app.h"
#include "doomgeneric.h"
#include "framebuffer.h"
#include "serial.h"
#include "timer.h"

app_result_t app_doom_main(void)
{
    uint32_t x = framebuffer_width() > 320 ? (framebuffer_width() - 320) / 2 : 0;
    uint32_t y = framebuffer_height() > 16 ? (framebuffer_height() - 16) / 2 : 0;

    framebuffer_clear(0x00000000);
    framebuffer_draw_text_scaled(x, y, "LAUNCHING DOOM", 0x00fff08a, 2);
    timer_sleep_ms(400);

    char *argv[] = { "doom", "-iwad", "C:\\DOOM\\DOOM1.WAD", "-nosound", 0 };
    DG_ResetQuit();
    serial_write("TinyDoomOS: doom app start\n");
    doomgeneric_Create(4, argv);
    serial_write("TinyDoomOS: doom app loop\n");

    while (!DG_ShouldQuit()) {
        doomgeneric_Tick();
    }

    serial_write("TinyDoomOS: doom app exit\n");
    return APP_RESULT_EXIT;
}
