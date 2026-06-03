#include <stdint.h>
#include "framebuffer.h"
#include "fs.h"
#include "gui.h"
#include "interrupts.h"
#include "memory.h"
#include "multiboot.h"
#include "process.h"
#include "serial.h"
#include "timer.h"

void kernel_main(uint32_t magic, multiboot_info_t *mbi)
{
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC || mbi == 0 || !(mbi->flags & (1 << 12))) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    serial_init();
    serial_write("TinyDoomOS: kernel_main\n");
    memory_init(mbi);
    serial_write("TinyDoomOS: memory ready\n");
    fs_init();
    serial_write("TinyDoomOS: fs ready\n");
    process_init();
    serial_write("TinyDoomOS: process table ready\n");

    framebuffer_init(mbi);
    framebuffer_clear(0x00000000);
    framebuffer_write_text("TINYDOOMOS STARTING...\n");
    serial_write("TinyDoomOS: framebuffer ready\n");

    timer_init(1000);
    serial_write("TinyDoomOS: timer ready\n");
    interrupts_init();
    serial_write("TinyDoomOS: interrupts ready\n");

    if (!gui_run()) {
        framebuffer_clear(0x00000000);
        framebuffer_draw_text_scaled(180, 180, "SYSTEM HALTED", 0x00fff08a, 2);
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }
}
