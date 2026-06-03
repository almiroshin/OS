#pragma once
#include <stddef.h>
#include <stdint.h>

#define PROCESS_PATH_MAX 64
#define PROCESS_NAME_MAX 32
#define PROCESS_DEFAULT_HEAP (16u * 1024u)

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_LOADING,
    PROCESS_RUNNING,
    PROCESS_EXITED,
} process_state_t;

struct process;
typedef void (*process_write_callback_t)(struct process *process, const char *text, void *ctx);

typedef struct process {
    int pid;
    process_state_t state;
    char name[PROCESS_NAME_MAX];
    char path[PROCESS_PATH_MAX];
    const unsigned char *image;
    size_t image_size;
    unsigned char *heap;
    size_t heap_size;
    size_t heap_used;
    int exit_code;
    process_write_callback_t write_callback;
    void *write_context;
} process_t;

void process_init(void);
process_t *process_create(const char *path, const unsigned char *image, size_t image_size, size_t heap_size);
void process_destroy(process_t *process);
void process_set_writer(process_t *process, process_write_callback_t callback, void *ctx);
void process_write(process_t *process, const char *text);
void *process_alloc(process_t *process, size_t size);
int process_validate_buffer(const process_t *process, const void *ptr, size_t size);
int process_active_count(void);
int process_total_started(void);
const process_t *process_get_slot(int index);

