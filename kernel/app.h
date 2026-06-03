#pragma once

typedef enum {
    APP_RESULT_EXIT = 0,
    APP_RESULT_HALT = 1,
} app_result_t;

typedef app_result_t (*app_entry_t)(void);

typedef struct {
    const char *id;
    const char *title;
    const char *summary;
    app_entry_t entry;
    const char *exec_path;
} app_descriptor_t;

int app_count(void);
const app_descriptor_t *app_get(int index);
const app_descriptor_t *app_find(const char *id);
app_result_t app_run(const app_descriptor_t *app);
