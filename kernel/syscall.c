#include "syscall.h"
#include "fs.h"
#include "memory.h"
#include "process.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

static int safe_write(process_t *process, const char *text, size_t length)
{
    char line[96];
    size_t n;

    if (!process_validate_buffer(process, text, length + 1)) {
        process_write(process, "SYSCALL REJECTED BAD POINTER");
        return -1;
    }

    n = length;
    if (n >= sizeof(line)) {
        n = sizeof(line) - 1;
    }

    memcpy(line, text, n);
    line[n] = 0;
    process_write(process, line);
    return 0;
}

static void list_callback(const fs_node_t *node, void *ctx)
{
    process_t *process = ctx;
    char line[96];
    const char *kind = node->type == FS_NODE_DIR ? "<DIR>" : "     ";

    snprintf(line, sizeof(line), "%s %u %s", kind, (unsigned int)node->size, node->path);
    process_write(process, line);
}

static int list_dir(process_t *process, const char *path, size_t length)
{
    char dir[64];
    size_t n = length;

    if (!process_validate_buffer(process, path, length + 1)) {
        process_write(process, "SYSCALL REJECTED BAD PATH");
        return -1;
    }

    if (n >= sizeof(dir)) {
        n = sizeof(dir) - 1;
    }

    memcpy(dir, path, n);
    dir[n] = 0;

    if (fs_list(dir, list_callback, process) != 0) {
        process_write(process, "DIR FAILED");
        return -1;
    }

    return 0;
}

static int memory_info(process_t *process)
{
    memory_stats_t stats;
    char line[96];

    memory_get_stats(&stats);
    snprintf(line, sizeof(line), "RAM %u KB  HEAP %u KB",
             stats.total_bytes / 1024,
             stats.heap_size / 1024);
    process_write(process, line);

    snprintf(line, sizeof(line), "HEAP USED %u KB  FREE %u KB",
             stats.heap_used / 1024,
             stats.heap_free / 1024);
    process_write(process, line);

    snprintf(line, sizeof(line), "ALLOCATIONS %u  HIGH WATER %u KB",
             stats.allocations,
             stats.high_water / 1024);
    process_write(process, line);
    return 0;
}

static int process_info(process_t *process)
{
    char line[96];

    snprintf(line, sizeof(line), "PID %d  SANDBOX HEAP %u/%u BYTES",
             process->pid,
             (unsigned int)process->heap_used,
             (unsigned int)process->heap_size);
    process_write(process, line);

    snprintf(line, sizeof(line), "IMAGE %u BYTES  PATH %s",
             (unsigned int)process->image_size,
             process->path);
    process_write(process, line);
    return 0;
}

int syscall_dispatch(process_t *process,
                     uint32_t number,
                     uintptr_t arg0,
                     uintptr_t arg1,
                     uintptr_t arg2,
                     uintptr_t arg3)
{
    (void)arg2;
    (void)arg3;

    if (!process) {
        return -1;
    }

    switch (number) {
    case SYS_EXIT:
        process->exit_code = (int)arg0;
        process->state = PROCESS_EXITED;
        return process->exit_code;
    case SYS_WRITE:
        return safe_write(process, (const char *)arg0, (size_t)arg1);
    case SYS_SLEEP:
        timer_sleep_ms((uint32_t)arg0);
        return 0;
    case SYS_LIST_DIR:
        return list_dir(process, (const char *)arg0, (size_t)arg1);
    case SYS_MEMORY_INFO:
        return memory_info(process);
    case SYS_PROCESS_INFO:
        return process_info(process);
    default:
        process_write(process, "UNKNOWN SYSCALL");
        return -1;
    }
}
