#include "app.h"

app_result_t app_doom_main(void);
app_result_t app_about_main(void);
app_result_t app_hello_main(void);
app_result_t app_command_main(void);
app_result_t app_notepad_main(void);
app_result_t app_calc_main(void);
app_result_t app_clock_main(void);
app_result_t app_paint_main(void);
app_result_t app_sysinfo_main(void);

const app_descriptor_t builtin_apps[] = {
    { "command", "COMMAND", "DOS STYLE SHELL", app_command_main, 0 },
    { "notepad", "NOTEPAD", "MEMORY TEXT EDITOR", app_notepad_main, 0 },
    { "calc", "CALC", "INTEGER CALCULATOR", app_calc_main, 0 },
    { "clock", "CLOCK", "UPTIME CLOCK", app_clock_main, 0 },
    { "paint", "PAINT", "KEYBOARD DRAWING", app_paint_main, 0 },
    { "sysinfo", "SYS INFO", "SYSTEM DETAILS", app_sysinfo_main, 0 },
    { "doom", "DOOM", "EMBEDDED SHAREWARE", app_doom_main, 0 },
    { "about", "ABOUT OS", "ARCHITECTURE INFO", app_about_main, 0 },
    { "hello", "HELLO", "SAMPLE APP MODULE", app_hello_main, 0 },
};

const int builtin_app_count = sizeof(builtin_apps) / sizeof(builtin_apps[0]);
