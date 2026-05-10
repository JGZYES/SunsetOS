#include "ata.h"

namespace ata {

static volatile bool disk_exists = false;
static volatile bool disk_checked = false;

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    asm volatile ("inb %1, %0" : "=a"(data) : "Nd"(port) : "memory");
    return data;
}

static inline void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" : : "a"(data), "Nd"(port) : "memory");
}

static inline uint16_t inw(uint16_t port) {
    uint16_t data;
    asm volatile ("inw %1, %0" : "=a"(data) : "Nd"(port) : "memory");
    return data;
}

static inline void outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" : : "a"(data), "Nd"(port) : "memory");
}

static void io_delay() {
    for (int i = 0; i < 4; i++) {
        inb(ATA_PRIMARY_IO + 7);
    }
}

static bool wait_for_ready() {
    uint32_t timeout = 100000;
    while (timeout > 0) {
        uint8_t status = inb(ATA_PRIMARY_IO + 7);
        if (!(status & ATA_STATUS_BUSY) && (status & ATA_STATUS_READY)) {
            return true;
        }
        timeout--;
    }
    return false;
}

static bool select_drive(uint16_t drive, uint32_t lba) {
    uint8_t head = 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F);
    outb(ATA_PRIMARY_IO + 6, head);
    io_delay();
    return true;
}

bool disk_present() {
    if (disk_checked) {
        return disk_exists;
    }

    outb(ATA_CONTROL_IO, 0x00);
    io_delay();

    outb(ATA_PRIMARY_IO + 6, 0xA0);
    io_delay();

    uint8_t cl = inb(ATA_PRIMARY_IO + 4);
    uint8_t ch = inb(ATA_PRIMARY_IO + 5);
    uint8_t status = inb(ATA_PRIMARY_IO + 7);

    if (status == 0xFF && cl == 0xFF && ch == 0xFF) {
        disk_exists = false;
    } else {
        outb(ATA_PRIMARY_IO + 7, 0xEC);
        io_delay();

        for (int i = 0; i < 10000; i++) {
            status = inb(ATA_PRIMARY_IO + 7);
            if ((status & ATA_STATUS_BUSY) == 0) {
                break;
            }
        }

        if ((status & ATA_STATUS_ERROR) != 0) {
            disk_exists = false;
        } else if ((status & ATA_STATUS_BUSY) != 0) {
            disk_exists = false;
        } else {
            disk_exists = true;
        }
    }

    disk_checked = true;
    return disk_exists;
}

void init() {
    disk_checked = false;
    disk_exists = false;
    disk_present();
}

int read_sectors(uint16_t drive, uint32_t lba, uint8_t count, uint8_t* buffer) {
    if (!disk_checked) {
        disk_present();
    }

    if (!disk_exists) {
        return -1;
    }

    if (count == 0 || count > 128) {
        return -2;
    }

    if (!wait_for_ready()) {
        return -3;
    }

    select_drive(drive, lba);
    outb(ATA_PRIMARY_IO + 2, count);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_IO + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_IO + 7, 0x20);

    if (!wait_for_ready()) {
        return -4;
    }

    uint8_t status = inb(ATA_PRIMARY_IO + 7);
    if (status & ATA_STATUS_ERROR) {
        return -5;
    }

    uint16_t* buf16 = reinterpret_cast<uint16_t*>(buffer);
    for (uint32_t i = 0; i < 256 * count; i++) {
        buf16[i] = inw(ATA_PRIMARY_IO);
    }

    return 0;
}

int write_sectors(uint16_t drive, uint32_t lba, uint8_t count, const uint8_t* buffer) {
    if (!disk_checked) {
        disk_present();
    }

    if (!disk_exists) {
        return -1;
    }

    if (count == 0 || count > 128) {
        return -2;
    }

    if (!wait_for_ready()) {
        return -3;
    }

    select_drive(drive, lba);
    outb(ATA_PRIMARY_IO + 2, count);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_IO + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_IO + 7, 0x30);

    if (!wait_for_ready()) {
        return -4;
    }

    const uint16_t* buf16 = reinterpret_cast<const uint16_t*>(buffer);
    for (uint32_t i = 0; i < 256 * count; i++) {
        outw(ATA_PRIMARY_IO, buf16[i]);
        io_delay();
    }

    return 0;
}

} // namespace ata
