#pragma once
#include "app.h"

int exec_app_count(void);
const app_descriptor_t *exec_app_get(int index);
app_result_t exec_run_app(const char *path, const char *title);

