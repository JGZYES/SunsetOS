#pragma once

#include <stdint.h>
#include <stddef.h>

namespace fat32 {

constexpr uint32_t SECTOR_SIZE = 512;

struct BPB {
    uint8_t jump[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved2;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} __attribute__((packed));

struct DirectoryEntry {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed));

constexpr uint8_t ATTR_READ_ONLY = 0x01;
constexpr uint8_t ATTR_HIDDEN = 0x02;
constexpr uint8_t ATTR_SYSTEM = 0x04;
constexpr uint8_t ATTR_VOLUME_ID = 0x08;
constexpr uint8_t ATTR_DIRECTORY = 0x10;
constexpr uint8_t ATTR_ARCHIVE = 0x20;

constexpr uint32_t FAT_EOC = 0x0FFFFFFF;

bool init();
bool is_ready();

int list_directory(const char* path, void (*callback)(const char*, uint8_t, uint32_t));
int create_directory(const char* path);
int create_file(const char* path, uint8_t attributes);
int delete_file(const char* path);
int read_file(const char* path, uint8_t* buffer, uint32_t max_size, uint32_t* bytes_read);
int write_file(const char* path, const uint8_t* data, uint32_t size);

const char* get_current_dir();
void set_current_dir(const char* path);

void print_filesystem_info();
void format_drive(uint16_t drive);

}
