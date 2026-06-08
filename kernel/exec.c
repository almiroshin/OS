#include "exec.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "fs.h"
#include "memory.h"
#include "process.h"
#include "syscall.h"
#include "timer.h"
#include "tinyos_app.h"
#include <stdio.h>
#include <string.h>

#define CONSOLE_LINES 18
#define CONSOLE_COLS 72
#define EXEC_DYNAMIC_MAX 24
#define EXEC_ID_LEN 32
#define EXEC_TITLE_LEN 32
#define EXEC_SUMMARY_LEN 32
#define EXEC_PATH_LEN 64

typedef struct {
    char title[32];
    char lines[CONSOLE_LINES][CONSOLE_COLS];
    int count;
    app_window_t win;
} exec_console_t;

static const app_descriptor_t exec_apps[] = {
    { "tap-welcome", "WELCOME.TAP", "LOADED FROM C:\\APPS", 0, "C:\\APPS\\WELCOME.TAP" },
    { "tap-files", "FILES.TAP", "C: DIRECTORY LIST", 0, "C:\\APPS\\FILES.TAP" },
    { "tap-memory", "MEMORY.TAP", "PROCESS MEMORY INFO", 0, "C:\\APPS\\MEMORY.TAP" },
    { "native-app", "NATIVE.APP", "RAW I386 APP IMAGE", 0, "C:\\APPS\\NATIVE.APP" },
};

static app_descriptor_t dynamic_exec_apps[EXEC_DYNAMIC_MAX];
static char dynamic_ids[EXEC_DYNAMIC_MAX][EXEC_ID_LEN];
static char dynamic_titles[EXEC_DYNAMIC_MAX][EXEC_TITLE_LEN];
static char dynamic_summaries[EXEC_DYNAMIC_MAX][EXEC_SUMMARY_LEN];
static char dynamic_paths[EXEC_DYNAMIC_MAX][EXEC_PATH_LEN];
static int dynamic_exec_count;
static process_t *native_current_process;

static int exec_static_count(void)
{
    return sizeof(exec_apps) / sizeof(exec_apps[0]);
}

static const char *exec_name_part(const char *path)
{
    const char *name = path;

    while (*path) {
        if (*path == '\\' || *path == '/') {
            name = path + 1;
        }
        path++;
    }
    return name;
}

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > text_len) {
        return 0;
    }
    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int is_static_exec_path(const char *path)
{
    for (int i = 0; i < exec_static_count(); i++) {
        if (strcmp(exec_apps[i].exec_path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void exec_dynamic_callback(const fs_node_t *node, void *ctx)
{
    int index;
    const char *name;
    const char *kind;

    (void)ctx;
    if (!node || node->type != FS_NODE_FILE || dynamic_exec_count >= EXEC_DYNAMIC_MAX) {
        return;
    }
    if (is_static_exec_path(node->path)) {
        return;
    }
    if (!ends_with(node->path, ".TAP") && !ends_with(node->path, ".APP")) {
        return;
    }

    index = dynamic_exec_count++;
    name = exec_name_part(node->path);
    kind = ends_with(node->path, ".APP") ? "DISK I386 APP" : "DISK TAP APP";

    snprintf(dynamic_ids[index], sizeof(dynamic_ids[index]), "disk-%u", (unsigned int)index);
    strncpy(dynamic_titles[index], name, sizeof(dynamic_titles[index]));
    dynamic_titles[index][sizeof(dynamic_titles[index]) - 1] = 0;
    strncpy(dynamic_summaries[index], kind, sizeof(dynamic_summaries[index]));
    dynamic_summaries[index][sizeof(dynamic_summaries[index]) - 1] = 0;
    strncpy(dynamic_paths[index], node->path, sizeof(dynamic_paths[index]));
    dynamic_paths[index][sizeof(dynamic_paths[index]) - 1] = 0;

    dynamic_exec_apps[index].id = dynamic_ids[index];
    dynamic_exec_apps[index].title = dynamic_titles[index];
    dynamic_exec_apps[index].summary = dynamic_summaries[index];
    dynamic_exec_apps[index].entry = 0;
    dynamic_exec_apps[index].exec_path = dynamic_paths[index];
}

static void exec_refresh_dynamic_apps(void)
{
    memset(dynamic_exec_apps, 0, sizeof(dynamic_exec_apps));
    memset(dynamic_ids, 0, sizeof(dynamic_ids));
    memset(dynamic_titles, 0, sizeof(dynamic_titles));
    memset(dynamic_summaries, 0, sizeof(dynamic_summaries));
    memset(dynamic_paths, 0, sizeof(dynamic_paths));
    dynamic_exec_count = 0;
    fs_list("C:\\APPS", exec_dynamic_callback, 0);
}

int exec_app_count(void)
{
    exec_refresh_dynamic_apps();
    return exec_static_count() + dynamic_exec_count;
}

const app_descriptor_t *exec_app_get(int index)
{
    int total;

    exec_refresh_dynamic_apps();
    total = exec_static_count() + dynamic_exec_count;
    if (index < 0 || index >= total) {
        return 0;
    }
    if (index < exec_static_count()) {
        return &exec_apps[index];
    }
    return &dynamic_exec_apps[index - exec_static_count()];
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
    const tinyos_app_header_t *header;

    if (!image || image_size < sizeof(tinyos_app_header_t)) {
        return 0;
    }

    header = (const tinyos_app_header_t *)image;
    return header->magic == TINYOS_APP_MAGIC
        && header->entry_offset >= sizeof(tinyos_app_header_t)
        && header->entry_offset < image_size
        && header->image_size <= image_size;
}

static int exec_native(process_t *process)
{
    const tinyos_app_header_t *header = (const tinyos_app_header_t *)process->image;
    tinyos_app_entry_t entry;
    tinyos_api_t api;
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

    entry = (tinyos_app_entry_t)(uintptr_t)(process->image + header->entry_offset);
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
