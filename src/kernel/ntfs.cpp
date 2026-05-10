#include "ntfs.h"
#include "vga.h"
#include "libc.h"
#include "ata.h"

namespace ntfs {

static NTFSBootSector boot_sector;
static uint16_t current_drive = 0;
static uint64_t mft_lcn;  // MFT 逻辑簇号
static uint64_t mft_mirror_lcn;
static uint32_t mft_record_size;
static uint8_t current_dir[256] = "/";
static bool fs_initialized = false;
static bool use_memory_fs = false;

// 内存文件系统结构（备用）
struct MemoryFile {
    char name[256];
    uint8_t attributes;
    uint8_t* data;
    uint32_t size;
    uint32_t capacity;
    MemoryFile* next;
};

static MemoryFile* root_directory = nullptr;

static uint64_t cluster_to_lba(uint64_t cluster) {
    return boot_sector.bpb.hidden_sectors + (cluster * boot_sector.bpb.sectors_per_cluster);
}

static bool read_sector(uint64_t lba, uint8_t* buffer) {
    return ata::read_sectors(current_drive, lba, 1, buffer) == 0;
}

static bool read_clusters(uint64_t lcn, uint32_t count, uint8_t* buffer) {
    uint64_t lba = cluster_to_lba(lcn);
    uint32_t sectors = count * boot_sector.bpb.sectors_per_cluster;
    return ata::read_sectors(current_drive, lba, sectors, buffer) == 0;
}

static bool read_mft_record(uint64_t record_number, uint8_t* buffer) {
    uint64_t lcn = mft_lcn + (record_number * mft_record_size / boot_sector.bpb.bytes_per_sector / boot_sector.bpb.sectors_per_cluster);
    uint64_t offset = (record_number * mft_record_size) % (boot_sector.bpb.bytes_per_sector * boot_sector.bpb.sectors_per_cluster);
    
    uint8_t cluster_buffer[4096];  // 足够大的缓冲区
    if (!read_clusters(lcn, 1, cluster_buffer)) {
        return false;
    }
    
    memcpy(buffer, cluster_buffer + offset, mft_record_size);
    return true;
}

static uint32_t get_mft_record_size() {
    if (boot_sector.bpb.sectors_per_cluster > 0 && boot_sector.bpb.sectors_per_cluster <= 0x80) {
        return boot_sector.bpb.sectors_per_cluster * boot_sector.bpb.bytes_per_sector;
    } else {
        return 1 << (-boot_sector.bpb.sectors_per_cluster);
    }
}

bool init() {
    vga::print("Initializing NTFS filesystem...\n");

    memset(&boot_sector, 0, sizeof(NTFSBootSector));
    
    ata::init();

    uint8_t buffer[SECTOR_SIZE];

    if (!ata::disk_present()) {
        vga::print("No disk present, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    vga::print("Disk detected, reading NTFS boot sector...\n");

    if (!read_sector(0, buffer)) {
        vga::print("Failed to read boot sector, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    memcpy(&boot_sector, buffer, sizeof(NTFSBootSector));

    // 检查是否是 NTFS
    if (memcmp(boot_sector.bpb.file_system_type, "NTFS    ", 8) != 0) {
        vga::print("Not an NTFS volume, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    vga::print("NTFS volume detected!\n");
    vga::print("  Bytes per sector: %u\n", boot_sector.bpb.bytes_per_sector);
    vga::print("  Sectors per cluster: %u\n", boot_sector.bpb.sectors_per_cluster);
    vga::print("  OEM ID: %.8s\n", boot_sector.bpb.oem_id);

    // 读取 $MFT 和 $MFTMirr 的位置（从引导扇区偏移 0x30）
    mft_lcn = *(uint64_t*)(buffer + 0x30);
    mft_mirror_lcn = *(uint64_t*)(buffer + 0x38);
    vga::print("  MFT LCN: %llx\n", mft_lcn);

    mft_record_size = get_mft_record_size();
    vga::print("  MFT record size: %u bytes\n", mft_record_size);

    fs_initialized = true;
    use_memory_fs = false;

    vga::print("NTFS filesystem initialized successfully!\n");

    return true;
}

bool is_ready() {
    return fs_initialized;
}

bool list_directory(const char* path, void (*callback)(const char*, uint8_t, uint32_t)) {
    if (!fs_initialized) return false;

    if (use_memory_fs) {
        MemoryFile* file = root_directory;
        while (file) {
            callback(file->name, file->attributes, file->size);
            file = file->next;
        }
        return true;
    }

    // TODO: 完整的 NTFS 目录列表实现
    // 简化版本：显示一些默认文件
    callback("$MFT", 0, 0);
    callback("$MFTMirr", 0, 0);
    callback("$LogFile", 0, 0);
    callback("$Volume", 0, 0);
    callback("$AttrDef", 0, 0);
    callback("$Root", 0, 0);
    callback("$Bitmap", 0, 0);
    callback("$Boot", 0, 0);
    callback("$BadClus", 0, 0);
    callback("$Secure", 0, 0);
    callback("$UpCase", 0, 0);
    callback("$Extend", 0, 0);
    
    return true;
}

bool create_file(const char* path, uint8_t attributes) {
    if (!fs_initialized) return false;

    if (use_memory_fs) {
        MemoryFile* file = new MemoryFile();
        strcpy(file->name, path);
        file->attributes = attributes;
        file->data = nullptr;
        file->size = 0;
        file->capacity = 0;
        file->next = root_directory;
        root_directory = file;
        return true;
    }

    // TODO: NTFS 文件创建
    vga::print("NTFS file creation not implemented yet\n");
    return false;
}

bool create_directory(const char* path) {
    return create_file(path, 0);
}

bool read_file(const char* path, uint8_t* buffer, uint32_t max_size, uint32_t* bytes_read) {
    if (!fs_initialized) return false;

    if (use_memory_fs) {
        MemoryFile* file = root_directory;
        while (file) {
            if (strcmp(file->name, path) == 0) {
                *bytes_read = (file->size < max_size) ? file->size : max_size;
                memcpy(buffer, file->data, *bytes_read);
                return true;
            }
            file = file->next;
        }
        return false;
    }

    // TODO: NTFS 文件读取
    *bytes_read = 0;
    return true;
}

bool write_file(const char* path, const uint8_t* data, uint32_t size) {
    if (!fs_initialized) return false;

    if (use_memory_fs) {
        MemoryFile* file = root_directory;
        while (file) {
            if (strcmp(file->name, path) == 0) {
                if (file->capacity < size) {
                    delete[] file->data;
                    file->data = new uint8_t[size];
                    file->capacity = size;
                }
                memcpy(file->data, data, size);
                file->size = size;
                return true;
            }
            file = file->next;
        }

        file = new MemoryFile();
        strcpy(file->name, path);
        file->attributes = 0;
        file->data = new uint8_t[size];
        memcpy(file->data, data, size);
        file->size = size;
        file->capacity = size;
        file->next = root_directory;
        root_directory = file;
        return true;
    }

    // TODO: NTFS 文件写入
    vga::print("NTFS file write not implemented yet\n");
    return false;
}

const char* get_current_dir() {
    return (const char*)current_dir;
}

bool set_current_dir(const char* path) {
    if (!fs_initialized) return false;
    strcpy((char*)current_dir, path);
    return true;
}

bool delete_file(const char* path) {
    if (!fs_initialized) return false;

    if (use_memory_fs) {
        MemoryFile* prev = nullptr;
        MemoryFile* current = root_directory;
        while (current) {
            if (strcmp(current->name, path) == 0) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    root_directory = current->next;
                }
                delete[] current->data;
                delete current;
                return true;
            }
            prev = current;
            current = current->next;
        }
        return false;
    }

    // TODO: NTFS 文件删除
    return false;
}

void print_filesystem_info() {
    if (!fs_initialized) {
        vga::print("Filesystem not initialized\n");
        return;
    }

    if (use_memory_fs) {
        vga::print("Memory Filesystem Info:\n");
        vga::print("  Type: In-Memory\n");
        vga::print("  Status: Active\n");
    } else {
        vga::print("NTFS Filesystem Info:\n");
        vga::print("  Bytes per sector: %u\n", boot_sector.bpb.bytes_per_sector);
        vga::print("  Sectors per cluster: %u\n", boot_sector.bpb.sectors_per_cluster);
        vga::print("  OEM ID: %.8s\n", boot_sector.bpb.oem_id);
        vga::print("  Serial Number: %llx\n", boot_sector.bpb.serial_number);
        vga::print("  MFT LCN: %llx\n", mft_lcn);
        vga::print("  MFTMirr LCN: %llx\n", mft_mirror_lcn);
        vga::print("  MFT Record Size: %u bytes\n", mft_record_size);
        vga::print("  FS Type: %.8s\n", boot_sector.bpb.file_system_type);
    }
}

void format_drive(uint16_t drive) {
    vga::print("Formatting drive %d as NTFS...\n", drive);
    vga::print("NTFS formatting not implemented yet\n");
}

} // namespace ntfs
