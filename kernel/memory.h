#pragma once
#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

typedef struct {
    uint32_t total_bytes;
    uint32_t heap_start;
    uint32_t heap_size;
    uint32_t heap_used;
    uint32_t heap_free;
    uint32_t allocations;
    uint32_t high_water;
} memory_stats_t;

void memory_init(const multiboot_info_t *mbi);
void *kmalloc(size_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t size);
size_t kmalloc_usable_size(const void *ptr);
void memory_get_stats(memory_stats_t *out);

