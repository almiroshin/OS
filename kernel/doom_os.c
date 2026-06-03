#include <stdint.h>
#include "doomgeneric.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "serial.h"
#include "timer.h"

static int doom_quit_requested;

void DG_Init(void)
{
    serial_write("TinyDoomOS: DG_Init\n");
}

void DG_DrawFrame(void)
{
    framebuffer_blit_doom((const uint32_t *) DG_ScreenBuffer,
                          DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

void DG_SleepMs(uint32_t ms)
{
    timer_sleep_ms(ms);
}

uint32_t DG_GetTicksMs(void)
{
    return timer_ticks_ms();
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    if (!keyboard_get_doom_key(pressed, key)) {
        return 0;
    }

    if (*pressed && *key == KEY_F10) {
        DG_Quit();
        return 0;
    }

    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    (void) title;
}

void DG_ResetQuit(void)
{
    doom_quit_requested = 0;
}

void DG_Quit(void)
{
    serial_write("TinyDoomOS: Doom requested quit\n");
    doom_quit_requested = 1;
}

int DG_ShouldQuit(void)
{
    return doom_quit_requested;
}
