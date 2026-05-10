#ifndef NTFS_H
#define NTFS_H

#include <stdint.h>
#include <stddef.h>

namespace ntfs {

constexpr uint32_t SECTOR_SIZE = 512;

// NTFS 引导扇区结构
#pragma pack(push, 1)
struct BIOSParameterBlock {
    uint8_t  jump[3];
    uint8_t  oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    // NTFS 扩展
    uint8_t  physical_drive;
    uint8_t  current_head;
    uint8_t  extended_boot_sig;
    uint64_t serial_number;
    uint8_t  volume_label[11];
    uint8_t  file_system_type[8];
};

struct NTFSBootSector {
    BIOSParameterBlock bpb;
    uint8_t boot_code[426];
    uint16_t signature;
};

// MFT 记录头
struct MFTRecordHeader {
    uint32_t magic;  // 'FILE'
    uint16_t update_sequence_offset;
    uint16_t update_sequence_size;
    uint64_t log_file_sequence_number;
    uint16_t sequence_number;
    uint16_t hard_link_count;
    uint16_t first_attribute_offset;
    uint16_t flags;
    uint32_t used_size;
    uint32_t allocated_size;
    uint64_t base_file_record;
    uint16_t next_attribute_id;
    uint16_t reserved;
    uint32_t record_number;
};

// 属性头
struct AttributeHeader {
    uint32_t type;
    uint32_t length;
    uint8_t non_resident;
    uint8_t name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attribute_id;
};

struct ResidentAttribute {
    AttributeHeader header;
    uint32_t data_size;
    uint16_t data_offset;
    uint8_t indexed_flag;
    uint8_t padding;
};

struct NonResidentAttribute {
    AttributeHeader header;
    uint64_t start_vcn;
    uint64_t last_vcn;
    uint16_t data_run_offset;
    uint16_t compression_unit;
    uint32_t padding;
    uint64_t allocated_size;
    uint64_t real_size;
    uint64_t initialized_size;
};

// 文件属性类型
#define ATTR_STANDARD_INFORMATION 0x10
#define ATTR_ATTRIBUTE_LIST       0x20
#define ATTR_FILE_NAME            0x30
#define ATTR_OBJECT_ID            0x40
#define ATTR_SECURITY_DESCRIPTOR  0x50
#define ATTR_VOLUME_NAME          0x60
#define ATTR_VOLUME_INFORMATION   0x70
#define ATTR_DATA                 0x80
#define ATTR_INDEX_ROOT           0x90
#define ATTR_INDEX_ALLOCATION     0xA0
#define ATTR_BITMAP               0xB0
#define ATTR_REPARSE_POINT        0xC0
#define ATTR_EA_INFORMATION       0xD0
#define ATTR_EA                   0xE0
#define ATTR_PROPERTY_SET         0xF0
#define ATTR_LOGGED_UTILITY_STREAM 0x100

// 文件名属性
struct FileNameAttribute {
    uint64_t parent_directory;
    uint64_t creation_time;
    uint64_t last_modification_time;
    uint64_t last_mft_change_time;
    uint64_t last_access_time;
    uint64_t allocated_size;
    uint64_t real_size;
    uint32_t file_attributes;
    uint32_t reparse_point_tag;
    uint8_t name_length;
    uint8_t namespace_type;
    uint16_t name[1]; // 可变长度 UTF-16
};

#pragma pack(pop)

// 函数声明
bool init();
bool is_ready();
bool list_directory(const char* path, void (*callback)(const char*, uint8_t, uint32_t));
bool create_file(const char* path, uint8_t attributes);
bool create_directory(const char* path);
bool read_file(const char* path, uint8_t* buffer, uint32_t max_size, uint32_t* bytes_read);
bool write_file(const char* path, const uint8_t* data, uint32_t size);
const char* get_current_dir();
bool set_current_dir(const char* path);
bool delete_file(const char* path);
void print_filesystem_info();
void format_drive(uint16_t drive);

} // namespace ntfs

#endif
