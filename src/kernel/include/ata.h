#pragma once

#include <stdint.h>

namespace ata {

constexpr uint16_t ATA_PRIMARY_IO = 0x1F0;
constexpr uint16_t ATA_CONTROL_IO = 0x3F6;
constexpr uint16_t ATA_PRIMARY_IRQ = 14;

constexpr uint8_t ATA_STATUS_BUSY = 0x80;
constexpr uint8_t ATA_STATUS_READY = 0x40;
constexpr uint8_t ATA_STATUS_ERROR = 0x01;

void init();
bool disk_present();
int read_sectors(uint16_t drive, uint32_t lba, uint8_t count, uint8_t* buffer);
int write_sectors(uint16_t drive, uint32_t lba, uint8_t count, const uint8_t* buffer);

} // namespace ata