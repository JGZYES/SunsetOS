#pragma once

#include <stdint.h>

namespace ata {

void init();
bool read_sector(uint16_t drive, uint32_t lba, uint8_t* buffer);
bool write_sector(uint16_t drive, uint32_t lba, const uint8_t* buffer);

}
