#pragma once

#include <stdint.h>

namespace ata {

void init();
void read_sector(uint16_t drive, uint32_t lba, uint8_t* buffer);
void write_sector(uint16_t drive, uint32_t lba, const uint8_t* buffer);

}
