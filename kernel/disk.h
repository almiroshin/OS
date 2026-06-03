#pragma once
#include <stdint.h>

#define DISK_SECTOR_SIZE 512u

void disk_init(void);
int disk_is_present(void);
uint32_t disk_sector_count(void);
int disk_read_sector(uint32_t lba, void *buffer);
int disk_write_sector(uint32_t lba, const void *buffer);

