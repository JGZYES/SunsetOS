#include "ata.h"

namespace ata {

constexpr uint16_t ATA_DATA = 0x1F0;
constexpr uint16_t ATA_SECCOUNT = 0x1F2;
constexpr uint16_t ATA_LBA_LOW = 0x1F3;
constexpr uint16_t ATA_LBA_MID = 0x1F4;
constexpr uint16_t ATA_LBA_HIGH = 0x1F5;
constexpr uint16_t ATA_DRIVE = 0x1F6;
constexpr uint16_t ATA_CMD = 0x1F7;

inline uint8_t inb(uint16_t port) {
    uint8_t data;
    asm volatile ("inb %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

inline void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

inline uint16_t inw(uint16_t port) {
    uint16_t data;
    asm volatile ("inw %1, %0" : "=a"(data) : "Nd"(port));
    return data;
}

inline void outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" : : "a"(data), "Nd"(port));
}

void init() {
}

void read_sector(uint16_t drive, uint32_t lba, uint8_t* buffer) {
    outb(ATA_DRIVE, static_cast<uint8_t>(0xE0 | ((drive & 1) << 4)));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, static_cast<uint8_t>(lba & 0xFF));
    outb(ATA_LBA_MID, static_cast<uint8_t>((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, static_cast<uint8_t>((lba >> 16) & 0xFF));
    outb(ATA_DRIVE, static_cast<uint8_t>(0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F)));
    outb(ATA_CMD, 0x20);

    while ((inb(ATA_CMD) & 0x80) != 0);

    for (int i = 0; i < 256; ++i) {
        uint16_t data = inw(ATA_DATA);
        buffer[i * 2] = static_cast<uint8_t>(data & 0xFF);
        buffer[i * 2 + 1] = static_cast<uint8_t>((data >> 8) & 0xFF);
    }
}

void write_sector(uint16_t drive, uint32_t lba, const uint8_t* buffer) {
    outb(ATA_DRIVE, static_cast<uint8_t>(0xE0 | ((drive & 1) << 4)));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW, static_cast<uint8_t>(lba & 0xFF));
    outb(ATA_LBA_MID, static_cast<uint8_t>((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, static_cast<uint8_t>((lba >> 16) & 0xFF));
    outb(ATA_DRIVE, static_cast<uint8_t>(0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F)));
    outb(ATA_CMD, 0x30);

    while ((inb(ATA_CMD) & 0x80) != 0);

    for (int i = 0; i < 256; ++i) {
        uint16_t data = static_cast<uint16_t>(buffer[i * 2] | (buffer[i * 2 + 1] << 8));
        outw(ATA_DATA, data);
    }
}

} // namespace ata
