#include <stddef.h>
#include <string.h>
#include "app.h"
#include "exec.h"

extern const app_descriptor_t builtin_apps[];
extern const int builtin_app_count;

int app_count(void)
{
    return builtin_app_count + exec_app_count();
}

const app_descriptor_t *app_get(int index)
{
    if (index < 0 || index >= app_count()) {
        return NULL;
    }
    if (index < builtin_app_count) {
        return &builtin_apps[index];
    }
    return exec_app_get(index - builtin_app_count);
}

const app_descriptor_t *app_find(const char *id)
{
    for (int i = 0; i < app_count(); i++) {
        const app_descriptor_t *app = app_get(i);
        if (app && strcmp(app->id, id) == 0) {
            return app;
        }
    }
    return NULL;
}

app_result_t app_run(const app_descriptor_t *app)
{
    if (!app) {
        return APP_RESULT_EXIT;
    }
    if (app->entry) {
        return app->entry();
    }
    if (app->exec_path) {
        return exec_run_app(app->exec_path, app->title);
    }
    return APP_RESULT_EXIT;
}
