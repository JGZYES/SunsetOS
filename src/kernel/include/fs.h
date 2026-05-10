#ifndef FS_H
#define FS_H

#include <stdint.h>

namespace fs {

enum FilesystemType {
    FS_NONE,
    FS_NTFS,
    FS_FAT32,
    FS_MEMORY
};

FilesystemType get_fs_type();
const char* get_fs_type_name();
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

} // namespace fs

#endif
