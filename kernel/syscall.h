#pragma once
#include <stdint.h>

typedef enum {
    SYS_EXIT = 1,
    SYS_WRITE = 2,
    SYS_SLEEP = 3,
    SYS_LIST_DIR = 4,
    SYS_MEMORY_INFO = 5,
    SYS_PROCESS_INFO = 6,
} syscall_number_t;

struct process;

int syscall_dispatch(struct process *process,
                     uint32_t number,
                     uintptr_t arg0,
                     uintptr_t arg1,
                     uintptr_t arg2,
                     uintptr_t arg3);

