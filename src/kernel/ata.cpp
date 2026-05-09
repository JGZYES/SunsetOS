#include "ata.h"
#include <stddef.h>

namespace ata {

static uint16_t* const ATA_DATA = reinterpret_cast<uint16_t*>(0x1F0);
static uint8_t* const ATA_ERROR = reinterpret_cast<uint8_t*>(0x1F1);
static uint8_t* const ATA_SECCOUNT = reinterpret_cast<uint8_t*>(0x1F2);
static uint8_t* const ATA_LBA_LOW = reinterpret_cast<uint8_t*>(0x1F3);
static uint8_t* const ATA_LBA_MID = reinterpret_cast<uint8_t*>(0x1F4);
static uint8_t* const ATA_LBA_HIGH = reinterpret_cast<uint8_t*>(0x1F5);
static uint8_t* const ATA_DRIVE = reinterpret_cast<uint8_t*>(0x1F6);
static uint8_t* const ATA_STATUS = reinterpret_cast<uint8_t*>(0x1F7);
static uint8_t* const ATA_CMD = reinterpret_cast<uint8_t*>(0x1F7);
static uint8_t* const ATA_CONTROL = reinterpret_cast<uint8_t*>(0x3F6);

static void io_wait() {
    for (volatile int i = 0; i < 4; i++) {
        asm volatile("inb $0x80, %%al" : : : "al");
    }
}

static void wait_ready() {
    for (int i = 0; i < 1000; i++) {
        uint8_t status = *ATA_STATUS;
        if ((status & 0x80) == 0) break;
        io_wait();
    }
    for (int i = 0; i < 1000; i++) {
        uint8_t status = *ATA_STATUS;
        if (status & 0x40) break;
        io_wait();
    }
}

void init() {
}

void read_sector(uint16_t drive, uint32_t lba, uint8_t* buffer) {
    *ATA_DRIVE = static_cast<uint8_t>(0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F));
    *ATA_SECCOUNT = 1;
    *ATA_LBA_LOW = lba & 0xFF;
    *ATA_LBA_MID = (lba >> 8) & 0xFF;
    *ATA_LBA_HIGH = (lba >> 16) & 0xFF;
    *ATA_CMD = 0x20;
    
    wait_ready();
    
    for (int i = 0; i < 256; i++) {
        uint16_t data = *ATA_DATA;
        buffer[i * 2] = data & 0xFF;
        buffer[i * 2 + 1] = (data >> 8) & 0xFF;
    }
}

void write_sector(uint16_t drive, uint32_t lba, const uint8_t* buffer) {
    *ATA_DRIVE = static_cast<uint8_t>(0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F));
    *ATA_SECCOUNT = 1;
    *ATA_LBA_LOW = lba & 0xFF;
    *ATA_LBA_MID = (lba >> 8) & 0xFF;
    *ATA_LBA_HIGH = (lba >> 16) & 0xFF;
    *ATA_CMD = 0x30;
    
    wait_ready();
    
    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        *ATA_DATA = data;
    }
    
    *ATA_CMD = 0xE7;
    wait_ready();
}

}
