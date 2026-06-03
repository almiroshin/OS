#include "process.h"
#include "memory.h"
#include <string.h>

#define MAX_PROCESSES 8

static process_t processes[MAX_PROCESSES];
static int next_pid = 1;
static int total_started;

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        src = "";
    }
    strncpy(dst, src, dst_size);
    dst[dst_size - 1] = 0;
}

void process_init(void)
{
    memset(processes, 0, sizeof(processes));
    next_pid = 1;
    total_started = 0;
}

process_t *process_create(const char *path, const unsigned char *image, size_t image_size, size_t heap_size)
{
    process_t *slot = 0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROCESS_UNUSED) {
            slot = &processes[i];
            break;
        }
    }

    if (!slot) {
        return 0;
    }

    memset(slot, 0, sizeof(*slot));
    slot->heap = kmalloc(heap_size);
    if (!slot->heap) {
        return 0;
    }

    slot->pid = next_pid++;
    slot->state = PROCESS_LOADING;
    copy_string(slot->path, sizeof(slot->path), path);
    copy_string(slot->name, sizeof(slot->name), path);
    slot->image = image;
    slot->image_size = image_size;
    slot->heap_size = heap_size;
    slot->heap_used = 0;
    slot->exit_code = 0;
    total_started++;
    return slot;
}

void process_destroy(process_t *process)
{
    if (!process) {
        return;
    }

    if (process->heap) {
        kfree(process->heap);
    }
    memset(process, 0, sizeof(*process));
}

void process_set_writer(process_t *process, process_write_callback_t callback, void *ctx)
{
    if (!process) {
        return;
    }
    process->write_callback = callback;
    process->write_context = ctx;
}

void process_write(process_t *process, const char *text)
{
    if (!process || !process->write_callback) {
        return;
    }
    process->write_callback(process, text ? text : "", process->write_context);
}

void *process_alloc(process_t *process, size_t size)
{
    size_t aligned;
    void *ptr;

    if (!process || !process->heap) {
        return 0;
    }

    aligned = (size + 15) & ~(size_t)15;
    if (process->heap_used + aligned > process->heap_size) {
        return 0;
    }

    ptr = process->heap + process->heap_used;
    process->heap_used += aligned;
    return ptr;
}

int process_validate_buffer(const process_t *process, const void *ptr, size_t size)
{
    uintptr_t start = (uintptr_t)ptr;
    uintptr_t end = start + size;
    uintptr_t image_start;
    uintptr_t image_end;
    uintptr_t heap_start;
    uintptr_t heap_end;

    if (!process || !ptr || end < start) {
        return 0;
    }

    image_start = (uintptr_t)process->image;
    image_end = image_start + process->image_size;
    heap_start = (uintptr_t)process->heap;
    heap_end = heap_start + process->heap_size;

    if (start >= image_start && end <= image_end) {
        return 1;
    }
    if (start >= heap_start && end <= heap_end) {
        return 1;
    }

    return 0;
}

int process_active_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROCESS_UNUSED) {
            count++;
        }
    }
    return count;
}

int process_total_started(void)
{
    return total_started;
}

const process_t *process_get_slot(int index)
{
    if (index < 0 || index >= MAX_PROCESSES) {
        return 0;
    }
    return &processes[index];
}

