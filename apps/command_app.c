#include <stdio.h>
#include <string.h>
#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "exec.h"
#include "framebuffer.h"
#include "fs.h"
#include "memory.h"
#include "timer.h"

#define CMD_LINES 18
#define CMD_COLS 74

typedef struct {
    char lines[CMD_LINES][CMD_COLS];
    int count;
    char input[CMD_COLS];
    int input_len;
    int running;
} command_state_t;

static char upper_char(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static void command_add(command_state_t *state, const char *text)
{
    if (state->count >= CMD_LINES) {
        for (int i = 1; i < CMD_LINES; i++) {
            strcpy(state->lines[i - 1], state->lines[i]);
        }
        state->count = CMD_LINES - 1;
    }

    strncpy(state->lines[state->count], text ? text : "", CMD_COLS);
    state->lines[state->count][CMD_COLS - 1] = 0;
    state->count++;
}

static void command_draw(command_state_t *state)
{
    char prompt[CMD_COLS + 8];
    app_window_t win = app_draw_window(640, 430, "COMMAND");

    framebuffer_draw_text(win.x + 18, win.y + 42, "TINYDOS COMMAND 0.3", 0x00fff08a);
    framebuffer_draw_text(win.x + 18, win.y + 58, "TYPE HELP FOR COMMANDS", 0x00b9c8d0);

    for (int i = 0; i < state->count; i++) {
        framebuffer_draw_text(win.x + 18, win.y + 84 + (uint32_t)i * 15,
                              state->lines[i], 0x00d7e4ea);
    }

    snprintf(prompt, sizeof(prompt), "C:\\>%s", state->input);
    framebuffer_fill_rect(win.x + 14, win.y + win.h - 38, win.w - 28, 18, 0x000d151c);
    framebuffer_draw_text(win.x + 18, win.y + win.h - 34, prompt, 0x00ffffff);
}

static const char *skip_spaces(const char *s)
{
    while (*s == ' ') {
        s++;
    }
    return s;
}

static void first_token(const char *s, char *out, size_t out_size, const char **rest)
{
    size_t n = 0;

    s = skip_spaces(s);
    while (*s && *s != ' ' && n + 1 < out_size) {
        out[n++] = *s++;
    }
    out[n] = 0;
    if (rest) {
        *rest = skip_spaces(s);
    }
}

typedef struct {
    command_state_t *state;
    int count;
} dir_context_t;

static const char *name_part(const char *path)
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

static void dir_callback(const fs_node_t *node, void *ctx)
{
    dir_context_t *dir = ctx;
    char line[CMD_COLS];
    const char *kind = node->type == FS_NODE_DIR ? "<DIR>" : "     ";

    snprintf(line, sizeof(line), "%s %u %s", kind, (unsigned int)node->size, name_part(node->path));
    command_add(dir->state, line);
    dir->count++;
}

static void command_dir(command_state_t *state, const char *path)
{
    char normalized[64];
    char line[CMD_COLS];
    dir_context_t ctx;

    if (!path || !*path) {
        path = "C:\\";
    }

    if (fs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        command_add(state, "BAD PATH");
        return;
    }

    snprintf(line, sizeof(line), "DIRECTORY OF %s", normalized);
    command_add(state, line);

    ctx.state = state;
    ctx.count = 0;
    if (fs_list(normalized, dir_callback, &ctx) != 0) {
        command_add(state, "DIR FAILED");
        return;
    }

    snprintf(line, sizeof(line), "%u ITEM(S)  %u FREE NODE(S)",
             (unsigned int)ctx.count,
             (unsigned int)fs_free_node_count());
    command_add(state, line);
}

static void command_type(command_state_t *state, const char *path)
{
    const unsigned char *data;
    size_t size;
    char line[CMD_COLS];
    size_t pos = 0;

    if (!path || !*path) {
        command_add(state, "TYPE NEEDS A FILE");
        return;
    }

    if (fs_read_file(path, &data, &size) != 0) {
        command_add(state, "FILE NOT FOUND");
        return;
    }

    while (pos < size) {
        size_t n = 0;
        while (pos < size && data[pos] != '\n' && n + 1 < sizeof(line)) {
            char c = (char)data[pos++];
            if (c != '\r') {
                line[n++] = c;
            }
        }
        while (pos < size && data[pos] != '\n') {
            pos++;
        }
        if (pos < size && data[pos] == '\n') {
            pos++;
        }
        line[n] = 0;
        command_add(state, line);
    }
}

static void command_write(command_state_t *state, const char *args, int append)
{
    char path[64];
    const char *text;
    char line[CMD_COLS];
    int result;

    first_token(args, path, sizeof(path), &text);
    if (!path[0] || !text || !*text) {
        command_add(state, append ? "APPEND FILE TEXT" : "WRITE FILE TEXT");
        return;
    }

    result = append
        ? fs_append_file(path, text, strlen(text))
        : fs_write_file(path, text, strlen(text));

    if (result != 0) {
        command_add(state, "WRITE FAILED");
        return;
    }

    snprintf(line, sizeof(line), "%s %u BYTES",
             append ? "APPENDED" : "WROTE",
             (unsigned int)strlen(text));
    command_add(state, line);
}

static void command_run(command_state_t *state, const char *arg)
{
    char path[64];
    char line[CMD_COLS];

    if (!arg || !*arg) {
        command_add(state, "RUN NEEDS A FILE");
        return;
    }

    if (strchr(arg, '\\') || strchr(arg, '/') || strchr(arg, ':')) {
        strncpy(path, arg, sizeof(path));
    } else {
        snprintf(path, sizeof(path), "C:\\APPS\\%s", arg);
    }
    path[sizeof(path) - 1] = 0;

    snprintf(line, sizeof(line), "RUN %s", path);
    command_add(state, line);
    command_draw(state);
    exec_run_app(path, name_part(path));
    command_add(state, "PROGRAM RETURNED");
}

static void command_mem(command_state_t *state)
{
    memory_stats_t stats;
    char line[CMD_COLS];

    memory_get_stats(&stats);
    snprintf(line, sizeof(line), "RAM %u KB  HEAP %u KB",
             stats.total_bytes / 1024,
             stats.heap_size / 1024);
    command_add(state, line);
    snprintf(line, sizeof(line), "USED %u KB  FREE %u KB  ALLOC %u",
             stats.heap_used / 1024,
             stats.heap_free / 1024,
             stats.allocations);
    command_add(state, line);
}

static void command_execute(command_state_t *state)
{
    char cmd[16];
    const char *args;
    char line[CMD_COLS + 8];

    snprintf(line, sizeof(line), "C:\\>%s", state->input);
    command_add(state, line);
    first_token(state->input, cmd, sizeof(cmd), &args);

    if (cmd[0] == 0) {
        return;
    } else if (strcmp(cmd, "HELP") == 0) {
        command_add(state, "DIR [PATH]   TYPE FILE   WRITE FILE TEXT");
        command_add(state, "APPEND FILE TEXT   DEL FILE   MKDIR PATH");
        command_add(state, "RUN FILE.APP/TAP   MEM   SYNC   CLS   VER   EXIT");
    } else if (strcmp(cmd, "VER") == 0) {
        command_add(state, "TINYDOOMOS 0.3 TINYFS EXEC BUILD");
        command_add(state, fs_is_persistent() ? "TINYFS DISK ONLINE" : "TINYFS VOLATILE MODE");
    } else if (strcmp(cmd, "DIR") == 0) {
        command_dir(state, args);
    } else if (strcmp(cmd, "TYPE") == 0) {
        command_type(state, args);
    } else if (strcmp(cmd, "WRITE") == 0) {
        command_write(state, args, 0);
    } else if (strcmp(cmd, "APPEND") == 0) {
        command_write(state, args, 1);
    } else if (strcmp(cmd, "DEL") == 0) {
        command_add(state, fs_delete(args) == 0 ? "DELETED" : "DELETE FAILED");
    } else if (strcmp(cmd, "MKDIR") == 0) {
        command_add(state, fs_mkdir(args) == 0 ? "DIRECTORY READY" : "MKDIR FAILED");
    } else if (strcmp(cmd, "RUN") == 0) {
        command_run(state, args);
    } else if (strcmp(cmd, "MEM") == 0) {
        command_mem(state);
    } else if (strcmp(cmd, "SYNC") == 0) {
        command_add(state, fs_flush() == 0 ? "SYNC OK" : "SYNC UNAVAILABLE");
    } else if (strcmp(cmd, "CLS") == 0) {
        state->count = 0;
    } else if (strcmp(cmd, "EXIT") == 0) {
        state->running = 0;
    } else {
        command_add(state, "BAD COMMAND");
    }
}

static int command_accept_key(command_state_t *state, unsigned char key)
{
    if (key == KEY_BACKSPACE) {
        if (state->input_len > 0) {
            state->input[--state->input_len] = 0;
        }
        return 1;
    }
    if (key == KEY_ENTER) {
        command_execute(state);
        state->input_len = 0;
        state->input[0] = 0;
        return 1;
    }
    if (key == KEY_ESCAPE) {
        state->running = 0;
        return 1;
    }

    if ((key >= 'a' && key <= 'z') ||
        (key >= 'A' && key <= 'Z') ||
        (key >= '0' && key <= '9') ||
        key == ' ' || key == '.' || key == '/' || key == '\\' ||
        key == '-' || key == '_' || key == '=') {
        if (state->input_len + 1 < CMD_COLS) {
            state->input[state->input_len++] = upper_char((char)key);
            state->input[state->input_len] = 0;
        }
        return 1;
    }

    return 0;
}

app_result_t app_command_main(void)
{
    command_state_t state;

    memset(&state, 0, sizeof(state));
    state.running = 1;
    command_add(&state, "WELCOME TO TINYDOS COMMAND");
    command_add(&state, fs_is_persistent() ? "TINYFS DISK ONLINE" : "TINYFS VOLATILE MODE");
    command_add(&state, "TRY DIR C:/APPS OR RUN NATIVE.APP");
    command_draw(&state);

    while (state.running) {
        unsigned char key;
        while (app_read_key(&key)) {
            if (command_accept_key(&state, key)) {
                command_draw(&state);
            }
        }
        timer_sleep_ms(10);
    }

    return APP_RESULT_EXIT;
}
