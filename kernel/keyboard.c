#include "keyboard.h"
#include "pic.h"
#include "ports.h"
#include "doomkeys.h"

#define QUEUE_SIZE 64

typedef struct {
    int pressed;
    unsigned char key;
} key_event_t;

static key_event_t queue[QUEUE_SIZE];
static volatile unsigned int head;
static volatile unsigned int tail;

static void push(int pressed, unsigned char key)
{
    unsigned int next = (head + 1) % QUEUE_SIZE;
    if (next != tail) {
        queue[head].pressed = pressed;
        queue[head].key = key;
        head = next;
    }
}

static unsigned char map_scancode(unsigned char sc)
{
    static const unsigned char normal[128] = {
        [0x01] = KEY_ESCAPE,
        [0x0E] = KEY_BACKSPACE,
        [0x1C] = KEY_ENTER,
        [0x39] = ' ',
        [0x3B] = KEY_F1,
        [0x3C] = KEY_F2,
        [0x3D] = KEY_F3,
        [0x3E] = KEY_F4,
        [0x3F] = KEY_F5,
        [0x40] = KEY_F6,
        [0x41] = KEY_F7,
        [0x42] = KEY_F8,
        [0x43] = KEY_F9,
        [0x44] = KEY_F10,
        [0x57] = KEY_F11,
        [0x58] = KEY_F12,
        [0x48] = KEY_UPARROW,
        [0x50] = KEY_DOWNARROW,
        [0x4B] = KEY_LEFTARROW,
        [0x4D] = KEY_RIGHTARROW,
        [0x1D] = KEY_FIRE,
        [0x2A] = KEY_RSHIFT,
        [0x36] = KEY_RSHIFT,
        [0x0F] = KEY_TAB,
        [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
        [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
        [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
        [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
        [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
        [0x18] = 'o', [0x19] = 'p', [0x1E] = 'a', [0x1F] = 's',
        [0x20] = 'd', [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
        [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x2C] = 'z',
        [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
        [0x31] = 'n', [0x32] = 'm',
        [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x37] = '*',
        [0x2B] = '\\',
        [0x4A] = '-', [0x4E] = '+',
    };
    return normal[sc & 0x7f];
}

void keyboard_irq(void)
{
    unsigned char sc = inb(0x60);
    unsigned char key = map_scancode(sc);
    if (key != 0) {
        push((sc & 0x80) == 0, key);
    }
    pic_eoi(1);
}

int keyboard_get_doom_key(int *pressed, unsigned char *key)
{
    if (tail == head) {
        return 0;
    }
    *pressed = queue[tail].pressed;
    *key = queue[tail].key;
    tail = (tail + 1) % QUEUE_SIZE;
    return 1;
}
