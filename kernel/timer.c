#include "timer.h"
#include "pic.h"
#include "ports.h"

static volatile uint32_t ticks;
static uint32_t tick_ms = 1;

void timer_init(uint32_t hz)
{
    uint32_t divisor = 1193180 / hz;
    tick_ms = 1000 / hz;
    if (tick_ms == 0) {
        tick_ms = 1;
    }
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xff);
    outb(0x40, (divisor >> 8) & 0xff);
}

uint32_t timer_ticks_ms(void)
{
    return ticks * tick_ms;
}

void timer_sleep_ms(uint32_t ms)
{
    uint32_t start = timer_ticks_ms();
    while (timer_ticks_ms() - start < ms) {
        __asm__ volatile ("hlt");
    }
}

void timer_irq(void)
{
    ticks++;
    pic_eoi(0);
}

