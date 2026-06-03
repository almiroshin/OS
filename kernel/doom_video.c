#include <string.h>
#include "d_event.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "i_video.h"
#include "serial.h"
#include "v_video.h"
#include "z_zone.h"

byte *I_VideoBuffer = NULL;
int usemouse = 0;
float mouse_acceleration = 2.0f;
int mouse_threshold = 10;
int vanilla_keyboard_mapping = 1;
boolean screensaver_mode = false;
boolean screenvisible = false;
int usegamma = 0;

static uint32_t palette32[256];

void I_InitGraphics(void)
{
    serial_write("TinyDoomOS: I_InitGraphics\n");
    I_VideoBuffer = Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);
    memset(I_VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT);
    screenvisible = true;
}

void I_ShutdownGraphics(void)
{
}

void I_StartFrame(void)
{
}

void I_StartTic(void)
{
    int pressed;
    unsigned char key;
    while (DG_GetKey(&pressed, &key)) {
        event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = pressed ? ev_keydown : ev_keyup;
        ev.data1 = key;
        if (key >= 32 && key <= 126) {
            ev.data2 = key;
        }
        D_PostEvent(&ev);
    }
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    static int frame_logged;
    if (!frame_logged) {
        serial_write("TinyDoomOS: first frame\n");
        frame_logged = 1;
    }
    for (int y = 0; y < SCREENHEIGHT; y++) {
        for (int x = 0; x < SCREENWIDTH; x++) {
            uint32_t color = palette32[I_VideoBuffer[y * SCREENWIDTH + x]];
            int dx = x * 2;
            int dy = y * 2;
            DG_ScreenBuffer[dy * DOOMGENERIC_RESX + dx] = color;
            DG_ScreenBuffer[dy * DOOMGENERIC_RESX + dx + 1] = color;
            DG_ScreenBuffer[(dy + 1) * DOOMGENERIC_RESX + dx] = color;
            DG_ScreenBuffer[(dy + 1) * DOOMGENERIC_RESX + dx + 1] = color;
        }
    }
    DG_DrawFrame();
}

void I_ReadScreen(byte *scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

void I_SetPalette(byte *palette)
{
    for (int i = 0; i < 256; i++) {
        uint8_t r = palette[i * 3 + 0];
        uint8_t g = palette[i * 3 + 1];
        uint8_t b = palette[i * 3 + 2];
        palette32[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}

int I_GetPaletteIndex(int r, int g, int b)
{
    int best = 0;
    int best_delta = 0x7fffffff;
    for (int i = 0; i < 256; i++) {
        int pr = (palette32[i] >> 16) & 0xff;
        int pg = (palette32[i] >> 8) & 0xff;
        int pb = palette32[i] & 0xff;
        int dr = pr - r;
        int dg = pg - g;
        int db = pb - b;
        int delta = dr * dr + dg * dg + db * db;
        if (delta < best_delta) {
            best_delta = delta;
            best = i;
        }
    }
    return best;
}

void I_BeginRead(void) {}
void I_EndRead(void) {}
void I_SetWindowTitle(char *title) { DG_SetWindowTitle(title); }
void I_GraphicsCheckCommandLine(void) {}
void I_SetGrabMouseCallback(grabmouse_callback_t func) { (void)func; }
void I_EnableLoadingDisk(void) {}
void I_BindVideoVariables(void) {}
void I_DisplayFPSDots(boolean dots_on) { (void)dots_on; }
void I_CheckIsScreensaver(void) {}
