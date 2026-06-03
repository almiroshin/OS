#include <stdint.h>
#include "interrupts.h"
#include "pic.h"

typedef struct {
    uint16_t base_lo;
    uint16_t selector;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_hi;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

extern void irq0_stub(void);
extern void irq1_stub(void);
extern void idt_load(idt_ptr_t *ptr);

static idt_entry_t idt[256];
static uint16_t code_selector;

static void idt_set_gate(uint8_t n, uint32_t handler)
{
    idt[n].base_lo = handler & 0xffff;
    idt[n].selector = code_selector;
    idt[n].always0 = 0;
    idt[n].flags = 0x8E;
    idt[n].base_hi = (handler >> 16) & 0xffff;
}

void interrupts_init(void)
{
    __asm__ volatile ("mov %%cs, %0" : "=r"(code_selector));
    pic_remap();
    idt_set_gate(32, (uint32_t) irq0_stub);
    idt_set_gate(33, (uint32_t) irq1_stub);

    idt_ptr_t ptr;
    ptr.limit = sizeof(idt) - 1;
    ptr.base = (uint32_t) idt;
    idt_load(&ptr);
    __asm__ volatile ("sti");
}
