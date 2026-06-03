#pragma once
#include <stdint.h>
void timer_init(uint32_t hz);
uint32_t timer_ticks_ms(void);
void timer_sleep_ms(uint32_t ms);
void timer_irq(void);

