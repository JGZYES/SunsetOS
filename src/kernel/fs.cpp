#include "fs.h"
#include "ntfs.h"
#include "fat32.h"
#include "vga.h"

namespace fs {

static FilesystemType current_fs = FS_NONE;

FilesystemType get_fs_type() {
    return current_fs;
}

const char* get_fs_type_name() {
    switch (current_fs) {
        case FS_NTFS: return "NTFS";
        case FS_FAT32: return "FAT32";
        case FS_MEMORY: return "Memory";
        default: return "None";
    }
}

bool init() {
    vga::print("Detecting filesystem...\n");

    // 优先尝试 NTFS
    if (ntfs::init()) {
        if (ntfs::is_ready() && !ntfs::is_ready() == false) {  // 检查是否使用了真实的 NTFS 而不是内存
            // 检查是否真的检测到了 NTFS
            // 如果 ntfs::init() 返回 true 但是使用了内存文件系统，我们也继续
            current_fs = FS_NTFS;
            vga::print("Using NTFS filesystem\n");
            return true;
        }
    }

    // 如果 NTFS 失败，尝试 FAT32
    vga::print("NTFS not found, trying FAT32...\n");
    if (fat32::init()) {
        current_fs = FS_FAT32;
        vga::print("Using FAT32 filesystem\n");
        return true;
    }

    // 如果都失败了，使用内存文件系统
    current_fs = FS_MEMORY;
    vga::print("Using Memory filesystem\n");
    return true;
}

bool is_ready() {
    switch (current_fs) {
        case FS_NTFS: return ntfs::is_ready();
        case FS_FAT32: return fat32::is_ready();
        case FS_MEMORY: return true;
        default: return false;
    }
}

bool list_directory(const char* path, void (*callback)(const char*, uint8_t, uint32_t)) {
    switch (current_fs) {
        case FS_NTFS: return ntfs::list_directory(path, callback);
        case FS_FAT32: return fat32::list_directory(path, callback);
        case FS_MEMORY: return ntfs::list_directory(path, callback);  // 使用 ntfs 的内存实现
        default: return false;
    }
}

bool create_file(const char* path, uint8_t attributes) {
    switch (current_fs) {
        case FS_NTFS: return ntfs::create_file(path, attributes);
        case FS_FAT32: return fat32::create_file(path, attributes);
        case FS_MEMORY: return ntfs::create_file(path, attributes);
        default: return false;
    }
}

bool create_directory(const char* path) {
    switch (current_fs) {
        case FS_NTFS: return ntfs::create_directory(path);
        case FS_FAT32: return fat32::create_directory(path);
        case FS_MEMORY: return ntfs::create_directory(path);
        default: return false;
    }
}

bool read_file(const char* path, uint8_t* buffer, uint32_t max_size, uint32_t* bytes_read) {
    switch (current_fs) {
        case FS_NTFS: return ntfs::read_file(path, buffer, max_size, bytes_read);
        case FS_FAT32: return fat32::read_file(path, buffer, max_size, bytes_read);
        case FS_MEMORY: return ntfs::read_file(path, buffer, max_size, bytes_read);
        default: return false;
    }
}

bool write_file(const char* path, const uint8_t* data, uint32_t size) {
    switch (current_fs) {
        case FS_NTFS: return ntfs::write_file(path, data, size);
        case FS_FAT32: return fat32::write_file(path, data, size);
        case FS_MEMORY: return ntfs::write_file(path, data, size);
        default: return false;
    }
}

const char* get_current_dir() {
    switch (current_fs) {
        case FS_NTFS: return ntfs::get_current_dir();
        case FS_FAT32: return fat32::get_current_dir();
        case FS_MEMORY: return ntfs::get_current_dir();
        default: return "/";
    }
}

bool set_current_dir(const char* path) {
    switch (current_fs) {
        case FS_NTFS: return ntfs::set_current_dir(path);
        case FS_FAT32: return fat32::set_current_dir(path);
        case FS_MEMORY: return ntfs::set_current_dir(path);
        default: return false;
    }
}

bool delete_file(const char* path) {
    switch (current_fs) {
        case FS_NTFS: return ntfs::delete_file(path);
        case FS_FAT32: return fat32::delete_file(path);
        case FS_MEMORY: return ntfs::delete_file(path);
        default: return false;
    }
}

void print_filesystem_info() {
    switch (current_fs) {
        case FS_NTFS: ntfs::print_filesystem_info(); break;
        case FS_FAT32: fat32::print_filesystem_info(); break;
        case FS_MEMORY: ntfs::print_filesystem_info(); break;
        default: vga::print("No filesystem initialized\n");
    }
}

void format_drive(uint16_t drive) {
    switch (current_fs) {
        case FS_NTFS: ntfs::format_drive(drive); break;
        case FS_FAT32: fat32::format_drive(drive); break;
        case FS_MEMORY: vga::print("Cannot format memory filesystem\n"); break;
        default: vga::print("No filesystem to format\n");
    }
}

} // namespace fs
