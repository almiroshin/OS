#include <stdio.h>
#include <string.h>
#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "exec.h"
#include "framebuffer.h"
#include "fs.h"
#include "timer.h"

#define FM_MAX_ENTRIES 48
#define FM_PATH_LEN 64
#define FM_NAME_LEN 32
#define FM_ROW_H 18
#define FM_VIEW_LINES 18
#define FM_TEXT_LINE 86

typedef struct {
    char path[FM_PATH_LEN];
    char name[FM_NAME_LEN];
    fs_node_type_t type;
    size_t size;
} fm_entry_t;

typedef struct {
    char path[FM_PATH_LEN];
    fm_entry_t entries[FM_MAX_ENTRIES];
    int count;
    int selected;
    int top;
    char status[80];
} fm_state_t;

typedef struct {
    fm_state_t *state;
    int count;
} fm_list_ctx_t;

static char upper_char(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static int is_root_path(const char *path)
{
    return strcmp(path, "C:\\") == 0;
}

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

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > text_len) {
        return 0;
    }
    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int is_exec_file(const fm_entry_t *entry)
{
    return entry && entry->type == FS_NODE_FILE &&
           (ends_with(entry->path, ".TAP") || ends_with(entry->path, ".APP"));
}

static int is_text_file(const fm_entry_t *entry)
{
    return entry && entry->type == FS_NODE_FILE &&
           (ends_with(entry->path, ".TXT") || ends_with(entry->path, ".TAP"));
}

static void parent_path(const char *path, char *out, size_t out_size)
{
    size_t slash = 0;
    size_t len = strlen(path);

    if (!out || out_size < 4 || is_root_path(path)) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        if (path[i] == '\\') {
            slash = i;
        }
    }

    if (slash <= 2) {
        strcpy(out, "C:\\");
        return;
    }

    if (slash >= out_size) {
        return;
    }
    memcpy(out, path, slash);
    out[slash] = 0;
}

static void make_child_path(const char *parent, const char *name, char *out, size_t out_size)
{
    if (is_root_path(parent)) {
        snprintf(out, out_size, "C:\\%s", name);
    } else {
        snprintf(out, out_size, "%s\\%s", parent, name);
    }
}

static void set_status(fm_state_t *state, const char *text)
{
    strncpy(state->status, text ? text : "", sizeof(state->status));
    state->status[sizeof(state->status) - 1] = 0;
}

static void list_callback(const fs_node_t *node, void *ctx)
{
    fm_list_ctx_t *list = ctx;
    fm_entry_t *entry;

    if (!node || !list || list->state->count >= FM_MAX_ENTRIES) {
        return;
    }

    entry = &list->state->entries[list->state->count++];
    strncpy(entry->path, node->path, sizeof(entry->path));
    entry->path[sizeof(entry->path) - 1] = 0;
    strncpy(entry->name, name_part(node->path), sizeof(entry->name));
    entry->name[sizeof(entry->name) - 1] = 0;
    entry->type = node->type;
    entry->size = node->size;
}

static void fm_reload(fm_state_t *state)
{
    fm_list_ctx_t ctx;

    state->count = 0;
    ctx.state = state;
    ctx.count = 0;

    if (fs_list(state->path, list_callback, &ctx) != 0) {
        set_status(state, "DIRECTORY READ FAILED");
        return;
    }

    if (state->selected >= state->count) {
        state->selected = state->count - 1;
    }
    if (state->selected < 0) {
        state->selected = 0;
    }
    if (state->top < 0) {
        state->top = 0;
    }
    if (state->top > state->selected) {
        state->top = state->selected;
    }
}

static void keep_visible(fm_state_t *state, int visible)
{
    if (state->selected < state->top) {
        state->top = state->selected;
    }
    if (state->selected >= state->top + visible) {
        state->top = state->selected - visible + 1;
    }
    if (state->top < 0) {
        state->top = 0;
    }
}

static void draw_side_panel(const app_window_t *win, const fm_state_t *state)
{
    uint32_t panel_x = win->x + win->w - 244;
    uint32_t panel_y = win->y + 82;
    const fm_entry_t *entry = state->count > 0 ? &state->entries[state->selected] : 0;
    char line[80];

    framebuffer_fill_rect(panel_x, panel_y, 214, 252, 0x00131b23);
    framebuffer_draw_rect(panel_x, panel_y, 214, 252, 0x004a6575);
    framebuffer_draw_text(panel_x + 14, panel_y + 14, "ITEM INFO", 0x00fff08a);

    if (!entry) {
        framebuffer_draw_text(panel_x + 14, panel_y + 42, "EMPTY DIRECTORY", 0x00d7e4ea);
        return;
    }

    framebuffer_draw_text(panel_x + 14, panel_y + 42, entry->name, 0x00ffffff);
    snprintf(line, sizeof(line), "%s  %u BYTES",
             entry->type == FS_NODE_DIR ? "DIRECTORY" : "FILE",
             (unsigned int)entry->size);
    framebuffer_draw_text(panel_x + 14, panel_y + 66, line, 0x00d7e4ea);

    if (entry->type == FS_NODE_DIR) {
        framebuffer_draw_text(panel_x + 14, panel_y + 102, "ENTER OPEN", 0x00a7f3ff);
    } else if (is_exec_file(entry)) {
        framebuffer_draw_text(panel_x + 14, panel_y + 102, "ENTER RUN", 0x00a7f3ff);
    } else if (is_text_file(entry)) {
        framebuffer_draw_text(panel_x + 14, panel_y + 102, "ENTER VIEW", 0x00a7f3ff);
    } else {
        framebuffer_draw_text(panel_x + 14, panel_y + 102, "NO DEFAULT ACTION", 0x006f8792);
    }

    if (entry->type == FS_NODE_FILE) {
        framebuffer_draw_text(panel_x + 14, panel_y + 126, "D DELETE FILE", 0x00d7e4ea);
    }
    framebuffer_draw_text(panel_x + 14, panel_y + 158, "N NEW FOLDER", 0x00d7e4ea);
    framebuffer_draw_text(panel_x + 14, panel_y + 182, "BACKSPACE UP", 0x00d7e4ea);
    framebuffer_draw_text(panel_x + 14, panel_y + 206, "R REFRESH", 0x00d7e4ea);
}

static void fm_draw(fm_state_t *state)
{
    app_window_t win = app_draw_window(760, 468, "FILE MANAGER");
    int visible = (int)((win.h - 136) / FM_ROW_H);
    uint32_t list_x = win.x + 24;
    uint32_t list_y = win.y + 100;
    uint32_t list_w = win.w > 300 ? win.w - 292 : win.w - 48;
    char line[96];

    if (visible < 1) {
        visible = 1;
    }
    keep_visible(state, visible);

    framebuffer_draw_text_scaled(win.x + 26, win.y + 46, "C: FILES", 0x00fff08a, 2);
    framebuffer_draw_text(win.x + 28, win.y + 76, state->path, 0x00a7f3ff);

    framebuffer_fill_rect(list_x, list_y - 18, list_w, 18, 0x00242f3d);
    framebuffer_draw_text(list_x + 8, list_y - 14, "TYPE  SIZE  NAME", 0x00ffffff);

    for (int row = 0; row < visible; row++) {
        int index = state->top + row;
        uint32_t y = list_y + (uint32_t)row * FM_ROW_H;
        uint32_t bg = index == state->selected ? 0x002a6f92 : 0x00131b23;
        uint32_t fg = index == state->selected ? 0x00ffffff : 0x00d7e4ea;

        framebuffer_fill_rect(list_x, y, list_w, FM_ROW_H - 1, bg);
        if (index >= state->count) {
            continue;
        }

        snprintf(line, sizeof(line), "%s  %u  %s",
                 state->entries[index].type == FS_NODE_DIR ? "<DIR>" : "FILE",
                 (unsigned int)state->entries[index].size,
                 state->entries[index].name);
        framebuffer_draw_text(list_x + 8, y + 5, line, fg);
    }

    draw_side_panel(&win, state);

    if (state->count == 0) {
        framebuffer_draw_text(list_x + 8, list_y + 10, "NO FILES", 0x00d7e4ea);
    }

    framebuffer_fill_rect(win.x + 18, win.y + win.h - 42, win.w - 36, 20, 0x000d151c);
    framebuffer_draw_text(win.x + 24, win.y + win.h - 36, state->status, 0x00b9c8d0);
    app_draw_footer("ARROWS SELECT   ENTER OPEN/RUN/VIEW   BACKSPACE UP   N NEW DIR   D DELETE   ESC RETURN");
}

static void extract_text_line(const unsigned char *data, size_t size, int wanted, char *out, size_t out_size)
{
    size_t pos = 0;
    int line = 0;

    out[0] = 0;
    while (pos < size) {
        size_t len = 0;
        if (line == wanted) {
            while (pos < size && data[pos] != '\n' && len + 1 < out_size) {
                char c = (char)data[pos++];
                if (c != '\r') {
                    out[len++] = c;
                }
            }
            out[len] = 0;
            return;
        }
        while (pos < size && data[pos] != '\n') {
            pos++;
        }
        if (pos < size && data[pos] == '\n') {
            pos++;
        }
        line++;
    }
}

static int count_text_lines(const unsigned char *data, size_t size)
{
    int lines = size > 0 ? 1 : 0;

    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

static void draw_text_viewer(const char *title, const unsigned char *data, size_t size, int scroll)
{
    app_window_t win = app_draw_window(720, 430, title);
    char line[FM_TEXT_LINE];
    char meta[80];

    snprintf(meta, sizeof(meta), "%u BYTES", (unsigned int)size);
    framebuffer_draw_text(win.x + 24, win.y + 46, meta, 0x00fff08a);

    for (int i = 0; i < FM_VIEW_LINES; i++) {
        extract_text_line(data, size, scroll + i, line, sizeof(line));
        framebuffer_draw_text(win.x + 24, win.y + 76 + (uint32_t)i * 18, line, 0x00d7e4ea);
    }

    framebuffer_draw_text(win.x + 24, win.y + win.h - 32, "UP/DOWN SCROLL   ENTER OR ESC RETURN", 0x00a7f3ff);
}

static void view_file(const fm_entry_t *entry)
{
    const unsigned char *data;
    size_t size;
    int scroll = 0;
    int lines;

    if (!entry || fs_read_file(entry->path, &data, &size) != 0) {
        return;
    }

    lines = count_text_lines(data, size);
    draw_text_viewer(entry->name, data, size, scroll);

    for (;;) {
        unsigned char key;
        while (app_read_key(&key)) {
            if (key == KEY_ENTER || key == KEY_ESCAPE || key == ' ') {
                return;
            }
            if (key == KEY_DOWNARROW && scroll + 1 < lines) {
                scroll++;
                draw_text_viewer(entry->name, data, size, scroll);
            } else if (key == KEY_UPARROW && scroll > 0) {
                scroll--;
                draw_text_viewer(entry->name, data, size, scroll);
            }
        }
        timer_sleep_ms(10);
    }
}

static void prompt_new_dir(fm_state_t *state)
{
    char name[FM_NAME_LEN];
    int len = 0;
    int dirty = 1;

    memset(name, 0, sizeof(name));
    set_status(state, "NEW FOLDER NAME:");
    fm_draw(state);

    for (;;) {
        unsigned char key;

        if (dirty) {
            app_window_t win = app_draw_window(520, 160, "NEW FOLDER");
            framebuffer_draw_text(win.x + 30, win.y + 54, "TYPE NAME", 0x00d7e4ea);
            framebuffer_fill_rect(win.x + 30, win.y + 82, win.w - 60, 20, 0x000d151c);
            framebuffer_draw_text(win.x + 36, win.y + 88, name, 0x00ffffff);
            framebuffer_draw_text(win.x + 30, win.y + 124, "ENTER CREATE   ESC CANCEL", 0x00a7f3ff);
            dirty = 0;
        }

        while (app_read_key(&key)) {
            if (key == KEY_ESCAPE) {
                set_status(state, "NEW FOLDER CANCELLED");
                return;
            }
            if (key == KEY_BACKSPACE) {
                if (len > 0) {
                    name[--len] = 0;
                    dirty = 1;
                }
            } else if (key == KEY_ENTER) {
                char path[FM_PATH_LEN];
                if (len == 0) {
                    set_status(state, "NEW FOLDER NEEDS A NAME");
                    return;
                }
                make_child_path(state->path, name, path, sizeof(path));
                if (fs_mkdir(path) == 0) {
                    set_status(state, "FOLDER CREATED");
                } else {
                    set_status(state, "CREATE FOLDER FAILED");
                }
                fm_reload(state);
                return;
            } else if (((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') ||
                        (key >= '0' && key <= '9') || key == '-' || key == '_') &&
                       len + 1 < (int)sizeof(name)) {
                name[len++] = upper_char((char)key);
                name[len] = 0;
                dirty = 1;
            }
        }
        timer_sleep_ms(10);
    }
}

static void confirm_delete(fm_state_t *state)
{
    fm_entry_t *entry;

    if (state->count <= 0) {
        return;
    }
    entry = &state->entries[state->selected];
    if (entry->type != FS_NODE_FILE) {
        set_status(state, "ONLY FILE DELETE IS SUPPORTED");
        return;
    }

    set_status(state, "DELETE FILE? Y CONFIRMS");
    fm_draw(state);

    for (;;) {
        unsigned char key;
        while (app_read_key(&key)) {
            if (key == 'y' || key == 'Y') {
                if (fs_delete(entry->path) == 0) {
                    set_status(state, "FILE DELETED");
                } else {
                    set_status(state, "DELETE FAILED");
                }
                fm_reload(state);
                return;
            }
            if (key == KEY_ESCAPE || key == 'n' || key == 'N' || key == KEY_ENTER) {
                set_status(state, "DELETE CANCELLED");
                return;
            }
        }
        timer_sleep_ms(10);
    }
}

static void open_selected(fm_state_t *state)
{
    fm_entry_t *entry;

    if (state->count <= 0) {
        return;
    }
    entry = &state->entries[state->selected];

    if (entry->type == FS_NODE_DIR) {
        strncpy(state->path, entry->path, sizeof(state->path));
        state->path[sizeof(state->path) - 1] = 0;
        state->selected = 0;
        state->top = 0;
        set_status(state, "DIRECTORY OPENED");
        fm_reload(state);
    } else if (is_exec_file(entry)) {
        exec_run_app(entry->path, entry->name);
        set_status(state, "PROGRAM RETURNED");
        fm_reload(state);
    } else if (is_text_file(entry)) {
        view_file(entry);
        set_status(state, "VIEWER CLOSED");
    } else {
        set_status(state, "NO DEFAULT ACTION FOR FILE");
    }
}

static void go_parent(fm_state_t *state)
{
    char parent[FM_PATH_LEN];

    if (is_root_path(state->path)) {
        set_status(state, "ALREADY AT C:\\");
        return;
    }

    memset(parent, 0, sizeof(parent));
    parent_path(state->path, parent, sizeof(parent));
    if (parent[0]) {
        strncpy(state->path, parent, sizeof(state->path));
        state->path[sizeof(state->path) - 1] = 0;
        state->selected = 0;
        state->top = 0;
        set_status(state, "PARENT DIRECTORY");
        fm_reload(state);
    }
}

app_result_t app_fileman_main(void)
{
    fm_state_t state;

    memset(&state, 0, sizeof(state));
    strcpy(state.path, "C:\\");
    set_status(&state, "READY");
    fm_reload(&state);
    fm_draw(&state);

    for (;;) {
        unsigned char key;
        while (app_read_key(&key)) {
            if (key == KEY_ESCAPE) {
                return APP_RESULT_EXIT;
            } else if (key == KEY_UPARROW) {
                if (state.selected > 0) {
                    state.selected--;
                }
            } else if (key == KEY_DOWNARROW) {
                if (state.selected + 1 < state.count) {
                    state.selected++;
                }
            } else if (key == KEY_ENTER || key == KEY_RIGHTARROW || key == ' ') {
                open_selected(&state);
            } else if (key == KEY_BACKSPACE || key == KEY_LEFTARROW) {
                go_parent(&state);
            } else if (key == 'r' || key == 'R') {
                fm_reload(&state);
                set_status(&state, "REFRESHED");
            } else if (key == 'd' || key == 'D') {
                confirm_delete(&state);
            } else if (key == 'n' || key == 'N') {
                prompt_new_dir(&state);
            }
            fm_draw(&state);
        }
        timer_sleep_ms(10);
    }
}
