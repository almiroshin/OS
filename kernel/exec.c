#include "exec.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "fs.h"
#include "memory.h"
#include "process.h"
#include "syscall.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

#define CONSOLE_LINES 18
#define CONSOLE_COLS 72
#define NATIVE_APP_MAGIC 0x31505041u

typedef struct {
    char title[32];
    char lines[CONSOLE_LINES][CONSOLE_COLS];
    int count;
    app_window_t win;
} exec_console_t;

typedef struct {
    uint32_t magic;
    uint32_t entry_offset;
    uint32_t image_size;
    uint32_t flags;
} native_app_header_t;

typedef struct {
    void (*write)(const char *text);
    void (*mem_info)(void);
    void (*proc_info)(void);
    void (*sleep)(uint32_t ms);
    int (*list_dir)(const char *path);
    void (*exit)(int code);
} native_app_api_t;

typedef int (*native_app_entry_t)(native_app_api_t *api);

static const app_descriptor_t exec_apps[] = {
    { "tap-welcome", "WELCOME.TAP", "LOADED FROM C:\\APPS", 0, "C:\\APPS\\WELCOME.TAP" },
    { "tap-files", "FILES.TAP", "C: DIRECTORY LIST", 0, "C:\\APPS\\FILES.TAP" },
    { "tap-memory", "MEMORY.TAP", "PROCESS MEMORY INFO", 0, "C:\\APPS\\MEMORY.TAP" },
    { "native-app", "NATIVE.APP", "RAW I386 APP IMAGE", 0, "C:\\APPS\\NATIVE.APP" },
};

static process_t *native_current_process;

int exec_app_count(void)
{
    return sizeof(exec_apps) / sizeof(exec_apps[0]);
}

const app_descriptor_t *exec_app_get(int index)
{
    if (index < 0 || index >= exec_app_count()) {
        return 0;
    }
    return &exec_apps[index];
}

static void console_draw(exec_console_t *console)
{
    console->win = app_draw_window(600, 382, console->title);
    framebuffer_draw_text(console->win.x + 20, console->win.y + 44,
                          "EXEC LOADER   PROCESS API   C: FS", 0x00fff08a);

    for (int i = 0; i < console->count; i++) {
        uint32_t color = i == console->count - 1 ? 0x00ffffff : 0x00d7e4ea;
        framebuffer_draw_text(console->win.x + 22,
                              console->win.y + 70 + (uint32_t)i * 15,
                              console->lines[i],
                              color);
    }

    framebuffer_draw_text(console->win.x + 22, console->win.y + console->win.h - 28,
                          "ENTER OR ESC RETURN", 0x00a7f3ff);
}

static void console_add(exec_console_t *console, const char *text)
{
    if (console->count >= CONSOLE_LINES) {
        for (int i = 1; i < CONSOLE_LINES; i++) {
            strcpy(console->lines[i - 1], console->lines[i]);
        }
        console->count = CONSOLE_LINES - 1;
    }

    strncpy(console->lines[console->count], text ? text : "", CONSOLE_COLS);
    console->lines[console->count][CONSOLE_COLS - 1] = 0;
    console->count++;
    console_draw(console);
}

static void process_console_write(process_t *process, const char *text, void *ctx)
{
    (void)process;
    console_add((exec_console_t *)ctx, text);
}

static void native_api_write(const char *text)
{
    if (!native_current_process || !text) {
        return;
    }
    syscall_dispatch(native_current_process, SYS_WRITE, (uintptr_t)text, strlen(text), 0, 0);
}

static void native_api_mem_info(void)
{
    if (native_current_process) {
        syscall_dispatch(native_current_process, SYS_MEMORY_INFO, 0, 0, 0, 0);
    }
}

static void native_api_proc_info(void)
{
    if (native_current_process) {
        syscall_dispatch(native_current_process, SYS_PROCESS_INFO, 0, 0, 0, 0);
    }
}

static void native_api_sleep(uint32_t ms)
{
    if (native_current_process) {
        syscall_dispatch(native_current_process, SYS_SLEEP, ms, 0, 0, 0);
    }
}

static int native_api_list_dir(const char *path)
{
    if (!native_current_process || !path) {
        return -1;
    }
    return syscall_dispatch(native_current_process, SYS_LIST_DIR, (uintptr_t)path, strlen(path), 0, 0);
}

static void native_api_exit(int code)
{
    if (native_current_process) {
        syscall_dispatch(native_current_process, SYS_EXIT, (uintptr_t)code, 0, 0, 0);
    }
}

static int starts_with(const char *line, const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static int parse_uint(const char *s)
{
    int value = 0;
    s = skip_spaces(s);
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }
    return value;
}

static char *copy_to_process(process_t *process, const char *text)
{
    size_t len = strlen(text) + 1;
    char *copy = process_alloc(process, len);

    if (!copy) {
        process_write(process, "PROCESS HEAP FULL");
        return 0;
    }

    memcpy(copy, text, len);
    return copy;
}

static void wait_for_return(void)
{
    for (;;) {
        unsigned char key;
        while (app_read_key(&key)) {
            if (key == KEY_ENTER || key == KEY_ESCAPE || key == ' ') {
                return;
            }
        }
        timer_sleep_ms(10);
    }
}

static void exec_wait_key(void)
{
    for (;;) {
        unsigned char key;
        while (app_read_key(&key)) {
            return;
        }
        timer_sleep_ms(10);
    }
}

static int run_command(process_t *process, const char *line)
{
    char *arg;

    line = skip_spaces(line);
    if (*line == 0 || *line == '#') {
        return 0;
    }

    if (starts_with(line, "PRINT ")) {
        const char *text = line + 6;
        arg = copy_to_process(process, text);
        if (!arg) {
            return -1;
        }
        syscall_dispatch(process, SYS_WRITE, (uintptr_t)arg, strlen(arg), 0, 0);
    } else if (starts_with(line, "DIR ")) {
        const char *path = skip_spaces(line + 4);
        arg = copy_to_process(process, path);
        if (!arg) {
            return -1;
        }
        syscall_dispatch(process, SYS_LIST_DIR, (uintptr_t)arg, strlen(arg), 0, 0);
    } else if (starts_with(line, "SLEEP ")) {
        syscall_dispatch(process, SYS_SLEEP, (uintptr_t)parse_uint(line + 6), 0, 0, 0);
    } else if (starts_with(line, "ALLOC ")) {
        int bytes = parse_uint(line + 6);
        char status[64];
        void *ptr = process_alloc(process, (size_t)bytes);
        snprintf(status, sizeof(status), "ALLOC %d BYTES %s", bytes, ptr ? "OK" : "FAILED");
        process_write(process, status);
    } else if (strcmp(line, "MEM") == 0) {
        syscall_dispatch(process, SYS_MEMORY_INFO, 0, 0, 0, 0);
    } else if (strcmp(line, "PROC") == 0) {
        syscall_dispatch(process, SYS_PROCESS_INFO, 0, 0, 0, 0);
    } else if (strcmp(line, "WAIT") == 0) {
        exec_wait_key();
    } else if (starts_with(line, "EXIT")) {
        syscall_dispatch(process, SYS_EXIT, (uintptr_t)parse_uint(line + 4), 0, 0, 0);
        return 1;
    } else {
        process_write(process, "UNKNOWN TAP COMMAND");
    }

    return 0;
}

static int exec_interpret(process_t *process)
{
    size_t pos = 0;
    char line[128];

    process->state = PROCESS_RUNNING;

    while (pos < process->image_size && process->state == PROCESS_RUNNING) {
        size_t len = 0;

        while (pos < process->image_size && process->image[pos] != '\n' && len + 1 < sizeof(line)) {
            char c = (char)process->image[pos++];
            if (c != '\r') {
                line[len++] = c;
            }
        }
        while (pos < process->image_size && process->image[pos] != '\n') {
            pos++;
        }
        if (pos < process->image_size && process->image[pos] == '\n') {
            pos++;
        }

        line[len] = 0;
        int result = run_command(process, line);
        if (result != 0) {
            return result > 0 ? 0 : result;
        }
    }

    if (process->state == PROCESS_RUNNING) {
        syscall_dispatch(process, SYS_EXIT, 0, 0, 0, 0);
    }

    return 0;
}

static int image_is_native_app(const unsigned char *image, size_t image_size)
{
    const native_app_header_t *header;

    if (!image || image_size < sizeof(native_app_header_t)) {
        return 0;
    }

    header = (const native_app_header_t *)image;
    return header->magic == NATIVE_APP_MAGIC
        && header->entry_offset >= sizeof(native_app_header_t)
        && header->entry_offset < image_size
        && header->image_size <= image_size;
}

static int exec_native(process_t *process)
{
    const native_app_header_t *header = (const native_app_header_t *)process->image;
    native_app_entry_t entry;
    native_app_api_t api;
    process_t *previous;

    if (!image_is_native_app(process->image, process->image_size)) {
        process_write(process, "BAD NATIVE APP HEADER");
        return -1;
    }

    memset(&api, 0, sizeof(api));
    api.write = native_api_write;
    api.mem_info = native_api_mem_info;
    api.proc_info = native_api_proc_info;
    api.sleep = native_api_sleep;
    api.list_dir = native_api_list_dir;
    api.exit = native_api_exit;

    process->state = PROCESS_RUNNING;
    previous = native_current_process;
    native_current_process = process;

    entry = (native_app_entry_t)(uintptr_t)(process->image + header->entry_offset);
    process_write(process, "NATIVE APP HEADER OK");
    entry(&api);

    if (process->state == PROCESS_RUNNING) {
        syscall_dispatch(process, SYS_EXIT, 0, 0, 0, 0);
    }

    native_current_process = previous;
    return 0;
}

app_result_t exec_run_app(const char *path, const char *title)
{
    const unsigned char *image;
    size_t image_size;
    process_t *process;
    exec_console_t console;

    memset(&console, 0, sizeof(console));
    strncpy(console.title, title ? title : "EXEC APP", sizeof(console.title));
    console.title[sizeof(console.title) - 1] = 0;
    console_draw(&console);

    if (fs_read_file(path, &image, &image_size) != 0) {
        console_add(&console, "EXEC FAILED FILE NOT FOUND");
        wait_for_return();
        return APP_RESULT_EXIT;
    }

    process = process_create(path, image, image_size, PROCESS_DEFAULT_HEAP);
    if (!process) {
        console_add(&console, "EXEC FAILED NO PROCESS SLOT");
        wait_for_return();
        return APP_RESULT_EXIT;
    }

    process_set_writer(process, process_console_write, &console);
    console_add(&console, "LOADER READ IMAGE FROM C: FS");
    if (image_is_native_app(image, image_size)) {
        exec_native(process);
    } else {
        exec_interpret(process);
    }
    console_add(&console, "PROCESS EXITED");
    wait_for_return();
    process_destroy(process);
    return APP_RESULT_EXIT;
}
