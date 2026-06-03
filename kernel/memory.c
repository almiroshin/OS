#include "memory.h"
#include <string.h>

#define HEAP_CAP_BYTES (32u * 1024u * 1024u)
#define MIN_HEAP_BYTES (1024u * 1024u)

extern char end;

typedef struct heap_block {
    uint32_t size;
    uint32_t free;
    struct heap_block *next;
    struct heap_block *prev;
} heap_block_t;

static heap_block_t *heap_first;
static uintptr_t heap_start;
static uintptr_t heap_limit;
static uint32_t total_memory_bytes;
static uint32_t heap_used_bytes;
static uint32_t heap_allocations;
static uint32_t heap_high_water;

static uintptr_t align_up_uintptr(uintptr_t value, uintptr_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static uintptr_t align_down_uintptr(uintptr_t value, uintptr_t align)
{
    return value & ~(align - 1);
}

static size_t align_up_size(size_t value, size_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static void split_block(heap_block_t *block, size_t size)
{
    uintptr_t next_addr = (uintptr_t)(block + 1) + size;
    size_t remaining = block->size - size;

    if (remaining <= sizeof(heap_block_t) + 32) {
        return;
    }

    heap_block_t *next = (heap_block_t *)next_addr;
    next->size = (uint32_t)(remaining - sizeof(heap_block_t));
    next->free = 1;
    next->next = block->next;
    next->prev = block;

    if (next->next) {
        next->next->prev = next;
    }

    block->size = (uint32_t)size;
    block->next = next;
}

static void coalesce(heap_block_t *block)
{
    if (block->next && block->next->free) {
        heap_block_t *next = block->next;
        block->size += sizeof(heap_block_t) + next->size;
        block->next = next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    if (block->prev && block->prev->free) {
        heap_block_t *prev = block->prev;
        prev->size += sizeof(heap_block_t) + block->size;
        prev->next = block->next;
        if (prev->next) {
            prev->next->prev = prev;
        }
    }
}

void memory_init(const multiboot_info_t *mbi)
{
    uintptr_t kernel_end = align_up_uintptr((uintptr_t)&end, 16);
    uintptr_t detected = 16u * 1024u * 1024u;

    if (mbi && (mbi->flags & 1)) {
        detected = (uintptr_t)(1024u + mbi->mem_upper) * 1024u;
    }

    total_memory_bytes = (uint32_t)detected;
    heap_start = kernel_end;
    heap_limit = detected;

    if (heap_limit > heap_start + HEAP_CAP_BYTES) {
        heap_limit = heap_start + HEAP_CAP_BYTES;
    }
    if (heap_limit < heap_start + MIN_HEAP_BYTES) {
        heap_limit = heap_start + MIN_HEAP_BYTES;
    }

    heap_limit = align_down_uintptr(heap_limit, 16);
    heap_first = (heap_block_t *)heap_start;
    heap_first->size = (uint32_t)(heap_limit - heap_start - sizeof(heap_block_t));
    heap_first->free = 1;
    heap_first->next = 0;
    heap_first->prev = 0;
    heap_used_bytes = 0;
    heap_allocations = 0;
    heap_high_water = 0;
}

void *kmalloc(size_t size)
{
    if (!heap_first) {
        return 0;
    }

    size = align_up_size(size ? size : 1, 16);

    for (heap_block_t *block = heap_first; block; block = block->next) {
        if (!block->free || block->size < size) {
            continue;
        }

        split_block(block, size);
        block->free = 0;
        heap_used_bytes += block->size;
        heap_allocations++;
        if (heap_used_bytes > heap_high_water) {
            heap_high_water = heap_used_bytes;
        }
        return block + 1;
    }

    return 0;
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }

    uintptr_t addr = (uintptr_t)ptr;
    if (addr < heap_start || addr >= heap_limit) {
        return;
    }

    heap_block_t *block = ((heap_block_t *)ptr) - 1;
    if (block->free) {
        return;
    }

    block->free = 1;
    heap_used_bytes -= block->size;
    if (heap_allocations > 0) {
        heap_allocations--;
    }
    coalesce(block);
}

size_t kmalloc_usable_size(const void *ptr)
{
    if (!ptr) {
        return 0;
    }

    uintptr_t addr = (uintptr_t)ptr;
    if (addr < heap_start || addr >= heap_limit) {
        return 0;
    }

    const heap_block_t *block = ((const heap_block_t *)ptr) - 1;
    return block->free ? 0 : block->size;
}

void *krealloc(void *ptr, size_t size)
{
    if (!ptr) {
        return kmalloc(size);
    }
    if (size == 0) {
        kfree(ptr);
        return 0;
    }

    size_t old_size = kmalloc_usable_size(ptr);
    if (old_size >= size) {
        return ptr;
    }

    void *next = kmalloc(size);
    if (!next) {
        return 0;
    }

    memcpy(next, ptr, old_size);
    kfree(ptr);
    return next;
}

void memory_get_stats(memory_stats_t *out)
{
    if (!out) {
        return;
    }

    uint32_t free_bytes = 0;
    for (heap_block_t *block = heap_first; block; block = block->next) {
        if (block->free) {
            free_bytes += block->size;
        }
    }

    out->total_bytes = total_memory_bytes;
    out->heap_start = (uint32_t)heap_start;
    out->heap_size = (uint32_t)(heap_limit - heap_start);
    out->heap_used = heap_used_bytes;
    out->heap_free = free_bytes;
    out->allocations = heap_allocations;
    out->high_water = heap_high_water;
}

