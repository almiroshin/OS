#include "disk.h"
#include "ports.h"
#include "serial.h"
#include <string.h>

#define ATA_DATA 0x1F0
#define ATA_ERROR 0x1F1
#define ATA_SECCOUNT0 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_HDDEVSEL 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7
#define ATA_CONTROL 0x3F6

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01
#define ATA_SR_DF 0x20

static int present;
static uint32_t sectors;

static void ata_select(uint32_t lba)
{
    outb(ATA_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();
}

static int ata_wait_not_busy(void)
{
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if ((status & ATA_SR_BSY) == 0) {
            return 0;
        }
    }
    return -1;
}

static int ata_wait_drq(void)
{
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status & (ATA_SR_ERR | ATA_SR_DF)) {
            return -1;
        }
        if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ)) {
            return 0;
        }
    }
    return -1;
}

static void ata_flush(void)
{
    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_not_busy();
}

void disk_init(void)
{
    uint16_t identify[256];

    present = 0;
    sectors = 0;

    outb(ATA_CONTROL, 0);
    ata_select(0);
    outb(ATA_SECCOUNT0, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    if (inb(ATA_STATUS) == 0) {
        serial_write("TinyDoomOS: no ATA disk\n");
        return;
    }

    if (ata_wait_drq() != 0) {
        serial_write("TinyDoomOS: ATA identify failed\n");
        return;
    }

    for (int i = 0; i < 256; i++) {
        identify[i] = inw(ATA_DATA);
    }

    sectors = ((uint32_t)identify[61] << 16) | identify[60];
    if (sectors == 0) {
        sectors = 8192;
    }

    present = 1;
    serial_write("TinyDoomOS: ATA disk ready\n");
}

int disk_is_present(void)
{
    return present;
}

uint32_t disk_sector_count(void)
{
    return sectors;
}

int disk_read_sector(uint32_t lba, void *buffer)
{
    uint16_t *out = buffer;

    if (!present || !buffer || lba >= sectors) {
        return -1;
    }

    ata_select(lba);
    outb(ATA_SECCOUNT0, 1);
    outb(ATA_LBA0, (uint8_t)(lba & 0xff));
    outb(ATA_LBA1, (uint8_t)((lba >> 8) & 0xff));
    outb(ATA_LBA2, (uint8_t)((lba >> 16) & 0xff));
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    for (int i = 0; i < 256; i++) {
        out[i] = inw(ATA_DATA);
    }
    return 0;
}

int disk_write_sector(uint32_t lba, const void *buffer)
{
    const uint16_t *in = buffer;

    if (!present || !buffer || lba >= sectors) {
        return -1;
    }

    ata_select(lba);
    outb(ATA_SECCOUNT0, 1);
    outb(ATA_LBA0, (uint8_t)(lba & 0xff));
    outb(ATA_LBA1, (uint8_t)((lba >> 8) & 0xff));
    outb(ATA_LBA2, (uint8_t)((lba >> 16) & 0xff));
    outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, in[i]);
    }
    ata_flush();
    return 0;
}

