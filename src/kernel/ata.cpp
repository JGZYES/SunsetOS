#include "ata.h"
#include <stddef.h>

namespace ata {

static const uint16_t ATA_DATA = 0x1F0;
static const uint16_t ATA_ERROR = 0x1F1;
static const uint16_t ATA_SECCOUNT = 0x1F2;
static const uint16_t ATA_LBA_LOW = 0x1F3;
static const uint16_t ATA_LBA_MID = 0x1F4;
static const uint16_t ATA_LBA_HIGH = 0x1F5;
static const uint16_t ATA_DRIVE = 0x1F6;
static const uint16_t ATA_STATUS = 0x1F7;
static const uint16_t ATA_CMD = 0x1F7;
static const uint16_t ATA_CONTROL = 0x3F6;

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    asm volatile ("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t data;
    asm volatile ("inw %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" : : "a"(data), "Nd"(port));
}

static inline void io_wait() {
    inb(0x80);
}

static uint8_t wait_bsy() {
    for (int i = 0; i < 10000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if ((status & 0x80) == 0) {
            return status;
        }
        io_wait();
    }
    return inb(ATA_STATUS);
}

static uint8_t wait_drq() {
    for (int i = 0; i < 10000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status & 0x40) {
            if (status & 0x08) {
                return status;
            }
        }
        io_wait();
    }
    return inb(ATA_STATUS);
}

void init() {
    outb(ATA_CONTROL, 0);
    for (volatile int i = 0; i < 1000; i++);
}

bool read_sector(uint16_t drive, uint32_t lba, uint8_t* buffer) {
    wait_bsy();

    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F)));
    io_wait();
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD, 0x24);

    uint8_t status = wait_drq();
    if ((status & 0x01) || (status & 0x20)) {
        return false;
    }

    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(ATA_DATA);
        buffer[i * 2] = data & 0xFF;
        buffer[i * 2 + 1] = (data >> 8) & 0xFF;
    }

    wait_bsy();
    return true;
}

bool write_sector(uint16_t drive, uint32_t lba, const uint8_t* buffer) {
    wait_bsy();

    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F)));
    io_wait();
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD, 0x34);

    uint8_t status = wait_drq();
    if ((status & 0x01) || (status & 0x20)) {
        return false;
    }

    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_DATA, data);
    }

    status = wait_bsy();
    if (status & 0x01) {
        return false;
    }

    outb(ATA_CMD, 0xE7);
    wait_bsy();

    return true;
}

}
