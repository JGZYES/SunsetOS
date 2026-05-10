#include "fat32.h"
#include "vga.h"
#include "libc.h"
#include "ata.h"

namespace fat32 {

static BPB bpb;
static uint16_t current_drive = 0;
static uint32_t partition_lba = 0;
static char current_path[256] = "/";
static bool fs_initialized = false;
static bool use_memory_fs = false;

struct MemoryFile {
    char name[256];
    uint8_t attributes;
    uint8_t* data;
    uint32_t size;
    uint32_t capacity;
    MemoryFile* next;
};

static MemoryFile* root_directory = nullptr;

static uint32_t get_total_sectors() {
    return bpb.total_sectors_16 != 0 ? bpb.total_sectors_16 : bpb.total_sectors_32;
}

static uint32_t get_fat_start_sector() {
    return partition_lba + bpb.reserved_sectors;
}

static uint32_t get_data_start_sector() {
    return get_fat_start_sector() + (bpb.num_fats * bpb.sectors_per_fat_32);
}

static uint32_t get_sectors_per_cluster() {
    return bpb.sectors_per_cluster;
}

static uint32_t get_bytes_per_cluster() {
    return bpb.bytes_per_sector * bpb.sectors_per_cluster;
}

static bool read_sector(uint32_t lba, uint8_t* buffer) {
    if (ata::read_sectors(current_drive, lba, 1, buffer) != 0) {
        return false;
    }
    return true;
}

static bool write_sector(uint32_t lba, const uint8_t* buffer) {
    if (ata::write_sectors(current_drive, lba, 1, buffer) != 0) {
        return false;
    }
    return true;
}

static uint32_t get_fat_entry(uint32_t cluster) {
    if (!fs_initialized || cluster < 2) return FAT_EOC;

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = get_fat_start_sector() + (fat_offset / SECTOR_SIZE);
    uint32_t offset_in_sector = fat_offset % SECTOR_SIZE;

    uint8_t buffer[SECTOR_SIZE];
    if (!read_sector(fat_sector, buffer)) {
        return FAT_EOC;
    }

    return *(uint32_t*)(buffer + offset_in_sector) & 0x0FFFFFFF;
}

static bool set_fat_entry(uint32_t cluster, uint32_t value) {
    if (!fs_initialized || cluster < 2) return false;

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = get_fat_start_sector() + (fat_offset / SECTOR_SIZE);
    uint32_t offset_in_sector = fat_offset % SECTOR_SIZE;

    uint8_t buffer[SECTOR_SIZE];
    if (!read_sector(fat_sector, buffer)) {
        return false;
    }

    *(uint32_t*)(buffer + offset_in_sector) = value & 0x0FFFFFFF;

    if (!write_sector(fat_sector, buffer)) {
        return false;
    }

    return true;
}

static uint32_t find_free_cluster() {
    if (!fs_initialized) return 0;

    uint32_t fat_sectors = bpb.sectors_per_fat_32;
    uint8_t buffer[SECTOR_SIZE];

    for (uint32_t sector = get_fat_start_sector(); sector < get_fat_start_sector() + fat_sectors; sector++) {
        if (!read_sector(sector, buffer)) {
            return 0;
        }

        for (uint32_t i = 0; i < SECTOR_SIZE; i += 4) {
            uint32_t entry = *(uint32_t*)(buffer + i) & 0x0FFFFFFF;
            if (entry == 0) {
                uint32_t cluster = ((sector - get_fat_start_sector()) * SECTOR_SIZE + i) / 4;
                if (set_fat_entry(cluster, FAT_EOC)) {
                    return cluster;
                }
                return 0;
            }
        }
    }

    return 0;
}

static void short_to_long_name(const uint8_t* short_name, char* long_name) {
    int j = 0;
    for (int i = 0; i < 11; i++) {
        if (short_name[i] == ' ') continue;
        if (i == 8) {
            long_name[j++] = '.';
        }
        long_name[j++] = short_name[i];
    }
    long_name[j] = '\0';
}

static void long_to_short_name(const char* name, uint8_t* short_name) {
    memset(short_name, ' ', 11);

    int i = 0;
    int j = 0;
    bool has_ext = false;
    int ext_start = 0;

    while (name[i] != '\0' && i < 12) {
        if (name[i] == '.') {
            has_ext = true;
            ext_start = i + 1;
            i++;
            continue;
        }
        if (!has_ext && j < 8) {
            short_name[j] = toupper(name[i]);
            j++;
        } else if (has_ext && (j - 8) < 3) {
            short_name[j] = toupper(name[i]);
            j++;
        }
        i++;
    }
}

static int find_directory_entry(const char* path, uint32_t* start_cluster, uint32_t* entry_index) {
    if (!fs_initialized) return -1;

    char path_copy[256];
    strcpy(path_copy, path);

    char* token = strtok(path_copy, "/");
    uint32_t current_cluster = bpb.root_cluster;

    if (!token) {
        *start_cluster = current_cluster;
        *entry_index = -1;
        return 0;
    }

    while (token) {
        uint8_t buffer[SECTOR_SIZE];
        uint32_t cluster = current_cluster;
        bool found = false;

        while (cluster != FAT_EOC && cluster != 0) {
            if (!read_sector(get_data_start_sector() + (cluster - 2) * get_sectors_per_cluster(), buffer)) {
                return -1;
            }

            DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer);
            for (int i = 0; i < (int)(SECTOR_SIZE / sizeof(DirectoryEntry)); i++) {
                if (entries[i].name[0] == 0x00) break;
                if (entries[i].name[0] == 0xE5) continue;

                char entry_name[13];
                short_to_long_name(entries[i].name, entry_name);

                if (strcmp(entry_name, token) == 0) {
                    current_cluster = (entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                    token = strtok(nullptr, "/");
                    found = true;
                    break;
                }
            }

            if (found) break;
            cluster = get_fat_entry(cluster);
        }

        if (!found) {
            return -1;
        }
    }

    *start_cluster = current_cluster;
    *entry_index = -1;
    return 0;
}

bool init() {
    vga::print("Initializing FAT32 filesystem...\n");

    memset(&bpb, 0, sizeof(bpb));
    bpb.bytes_per_sector = 512;
    bpb.sectors_per_cluster = 8;
    bpb.reserved_sectors = 32;
    bpb.num_fats = 2;
    bpb.sectors_per_fat_32 = 4096;
    bpb.root_cluster = 2;
    memcpy(bpb.fs_type, "FAT32   ", 8);
    memcpy(bpb.volume_label, "SUNSETOS   ", 11);

    ata::init();

    uint8_t buffer[SECTOR_SIZE];

    if (!ata::disk_present()) {
        vga::print("No disk present, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    vga::print("Disk detected, reading MBR...\n");

    if (!read_sector(0, buffer)) {
        vga::print("Failed to read MBR, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    MBR* mbr = reinterpret_cast<MBR*>(buffer);

    if (mbr->signature != 0xAA55) {
        vga::print("Invalid MBR signature, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    uint32_t fat32_lba = 0;
    for (int i = 0; i < 4; i++) {
        vga::print("Partition %d: type=0x%02X, start_lba=%u\n", i, mbr->partitions[i].type, mbr->partitions[i].start_lba);
        if (mbr->partitions[i].type == 0x0B || mbr->partitions[i].type == 0x0C) {
            fat32_lba = mbr->partitions[i].start_lba;
            vga::print("Found FAT32 partition at LBA %u\n", fat32_lba);
            break;
        }
    }

    if (fat32_lba == 0) {
        vga::print("No FAT32 partition found, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    vga::print("Reading FAT32 BPB from LBA %u...\n", fat32_lba);

    if (!read_sector(fat32_lba, buffer)) {
        vga::print("Failed to read FAT32 BPB, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    memcpy(&bpb, buffer, sizeof(BPB));

    vga::print("FS Type: %.8s\n", bpb.fs_type);
    vga::print("Bytes per sector: %u\n", bpb.bytes_per_sector);
    vga::print("Sectors per cluster: %u\n", bpb.sectors_per_cluster);
    vga::print("Reserved sectors: %u\n", bpb.reserved_sectors);
    vga::print("Number of FATs: %u\n", bpb.num_fats);
    vga::print("Total sectors: %u\n", get_total_sectors());
    vga::print("Sectors per FAT: %u\n", bpb.sectors_per_fat_32);
    vga::print("Root cluster: %u\n", bpb.root_cluster);

    if (memcmp(bpb.fs_type, "FAT32   ", 8) != 0) {
        vga::print("Not a FAT32 filesystem, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }

    partition_lba = fat32_lba;
    fs_initialized = true;
    use_memory_fs = false;

    vga::print("FAT32 filesystem initialized successfully!\n");

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

    uint32_t start_cluster;
    uint32_t entry_index;

    if (find_directory_entry(path, &start_cluster, &entry_index) != 0) {
        return false;
    }

    uint8_t buffer[SECTOR_SIZE];
    uint32_t cluster = start_cluster;

    while (cluster != FAT_EOC && cluster != 0) {
        if (!read_sector(get_data_start_sector() + (cluster - 2) * get_sectors_per_cluster(), buffer)) {
            return false;
        }

        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer);
        for (int i = 0; i < (int)(SECTOR_SIZE / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;
            if ((entries[i].attributes & ATTR_VOLUME_ID) != 0) continue;
            if ((entries[i].attributes & ATTR_HIDDEN) != 0) continue;

            char name[13];
            short_to_long_name(entries[i].name, name);

            callback(name, entries[i].attributes, entries[i].file_size);
        }

        cluster = get_fat_entry(cluster);
    }

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

    char path_copy[256];
    strcpy(path_copy, path);

    char* last_slash = strrchr(path_copy, '/');
    const char* filename;
    char dir_path[256];

    if (last_slash) {
        *last_slash = '\0';
        strcpy(dir_path, path_copy);
        filename = last_slash + 1;
    } else {
        strcpy(dir_path, "/");
        filename = path_copy;
    }

    uint32_t dir_cluster;
    uint32_t dummy;

    if (find_directory_entry(dir_path, &dir_cluster, &dummy) != 0) {
        return false;
    }

    uint32_t cluster = find_free_cluster();
    if (cluster == 0) {
        return false;
    }

    uint8_t buffer[SECTOR_SIZE] = {0};
    for (uint32_t i = 0; i < get_sectors_per_cluster(); i++) {
        if (!write_sector(get_data_start_sector() + (cluster - 2) * get_sectors_per_cluster() + i, buffer)) {
            set_fat_entry(cluster, 0);
            return false;
        }
    }

    uint8_t dir_buffer[SECTOR_SIZE];
    uint32_t current_cluster = dir_cluster;

    while (current_cluster != FAT_EOC && current_cluster != 0) {
        if (!read_sector(get_data_start_sector() + (current_cluster - 2) * get_sectors_per_cluster(), dir_buffer)) {
            return false;
        }

        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(dir_buffer);
        for (int i = 0; i < (int)(SECTOR_SIZE / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                long_to_short_name(filename, entries[i].name);
                entries[i].attributes = attributes;
                entries[i].first_cluster_low = cluster & 0xFFFF;
                entries[i].first_cluster_high = (cluster >> 16) & 0xFFFF;
                entries[i].file_size = 0;

                if (!write_sector(get_data_start_sector() + (current_cluster - 2) * get_sectors_per_cluster(), dir_buffer)) {
                    return false;
                }

                return true;
            }
        }

        uint32_t next_cluster = get_fat_entry(current_cluster);
        if (next_cluster == FAT_EOC || next_cluster == 0) {
            uint32_t new_cluster = find_free_cluster();
            if (new_cluster == 0) {
                return false;
            }

            set_fat_entry(current_cluster, new_cluster);

            memset(dir_buffer, 0, SECTOR_SIZE);
            if (!write_sector(get_data_start_sector() + (new_cluster - 2) * get_sectors_per_cluster(), dir_buffer)) {
                return false;
            }

            current_cluster = new_cluster;
        } else {
            current_cluster = next_cluster;
        }
    }

    return false;
}

bool create_directory(const char* path) {
    return create_file(path, ATTR_DIRECTORY);
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

    uint32_t start_cluster;
    uint32_t entry_index;

    if (find_directory_entry(path, &start_cluster, &entry_index) != 0) {
        return false;
    }

    uint8_t sector_buffer[SECTOR_SIZE];
    uint32_t cluster = start_cluster;
    uint32_t bytes_read_so_far = 0;

    while (cluster != FAT_EOC && cluster != 0 && bytes_read_so_far < max_size) {
        for (uint32_t i = 0; i < get_sectors_per_cluster(); i++) {
            if (!read_sector(get_data_start_sector() + (cluster - 2) * get_sectors_per_cluster() + i, sector_buffer)) {
                return false;
            }

            uint32_t bytes_to_copy = (max_size - bytes_read_so_far < SECTOR_SIZE) ? (max_size - bytes_read_so_far) : SECTOR_SIZE;
            memcpy(buffer + bytes_read_so_far, sector_buffer, bytes_to_copy);
            bytes_read_so_far += bytes_to_copy;

            if (bytes_read_so_far >= max_size) {
                *bytes_read = bytes_read_so_far;
                return true;
            }
        }

        cluster = get_fat_entry(cluster);
    }

    *bytes_read = bytes_read_so_far;
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

    uint32_t start_cluster;
    uint32_t entry_index;

    if (find_directory_entry(path, &start_cluster, &entry_index) != 0) {
        if (!create_file(path, 0)) {
            return false;
        }
        if (find_directory_entry(path, &start_cluster, &entry_index) != 0) {
            return false;
        }
    }

    uint32_t total_clusters = (size + get_bytes_per_cluster() - 1) / get_bytes_per_cluster();
    uint32_t current_cluster = start_cluster;

    for (uint32_t i = 0; i < total_clusters; i++) {
        uint8_t sector_buffer[SECTOR_SIZE] = {0};

        for (uint32_t j = 0; j < get_sectors_per_cluster(); j++) {
            uint32_t offset = i * get_bytes_per_cluster() + j * SECTOR_SIZE;
            if (offset < size) {
                uint32_t bytes_to_copy = (size - offset < SECTOR_SIZE) ? (size - offset) : SECTOR_SIZE;
                memcpy(sector_buffer, data + offset, bytes_to_copy);
            }

            if (!write_sector(get_data_start_sector() + (current_cluster - 2) * get_sectors_per_cluster() + j, sector_buffer)) {
                return false;
            }
        }

        if (i < total_clusters - 1) {
            uint32_t next_cluster = find_free_cluster();
            if (next_cluster == 0) {
                return false;
            }
            set_fat_entry(current_cluster, next_cluster);
            current_cluster = next_cluster;
        }
    }

    return true;
}

const char* get_current_dir() {
    return current_path;
}

bool set_current_dir(const char* path) {
    if (!fs_initialized) return false;

    uint32_t start_cluster;
    uint32_t entry_index;

    if (find_directory_entry(path, &start_cluster, &entry_index) != 0) {
        return false;
    }

    strcpy(current_path, path);
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

    uint32_t start_cluster;
    uint32_t entry_index;

    if (find_directory_entry(path, &start_cluster, &entry_index) != 0) {
        return false;
    }

    uint8_t buffer[SECTOR_SIZE];
    uint32_t sector = get_data_start_sector() + (start_cluster - 2) * get_sectors_per_cluster();
    uint32_t offset_in_sector = (entry_index * sizeof(DirectoryEntry)) % SECTOR_SIZE;

    if (!read_sector(sector + entry_index / (SECTOR_SIZE / sizeof(DirectoryEntry)), buffer)) {
        return false;
    }

    DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(buffer) + (entry_index % (SECTOR_SIZE / sizeof(DirectoryEntry)));
    entry->name[0] = 0xE5;

    if (!write_sector(sector + entry_index / (SECTOR_SIZE / sizeof(DirectoryEntry)), buffer)) {
        return false;
    }

    return true;
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
        vga::print("FAT32 Filesystem Info:\n");
        vga::print("  Bytes per sector: %u\n", bpb.bytes_per_sector);
        vga::print("  Sectors per cluster: %u\n", bpb.sectors_per_cluster);
        vga::print("  Reserved sectors: %u\n", bpb.reserved_sectors);
        vga::print("  Number of FATs: %u\n", bpb.num_fats);
        vga::print("  Total sectors: %u\n", get_total_sectors());
        vga::print("  FAT size (sectors): %u\n", bpb.sectors_per_fat_32);
        vga::print("  Volume label: %.11s\n", bpb.volume_label);
        vga::print("  FS type: %.8s\n", bpb.fs_type);
    }
}

void format_drive(uint16_t drive) {
    vga::print("Formatting drive %d as FAT32...\n", drive);

    if (!ata::disk_present()) {
        vga::print("No disk present, cannot format\n");
        return;
    }

    uint32_t fat32_start = 2048;

    MBR mbr = {0};
    mbr.partitions[0].status = 0x80;
    mbr.partitions[0].type = 0x0C;
    mbr.partitions[0].start_lba = fat32_start;
    mbr.partitions[0].sector_count = 2097152;
    mbr.signature = 0xAA55;

    if (!write_sector(0, reinterpret_cast<uint8_t*>(&mbr))) {
        vga::print("Failed to write MBR\n");
        return;
    }

    uint8_t boot_sector[SECTOR_SIZE] = {0};

    BPB* new_bpb = reinterpret_cast<BPB*>(boot_sector);
    new_bpb->jump[0] = 0xEB;
    new_bpb->jump[1] = 0x3C;
    new_bpb->jump[2] = 0x90;
    memcpy(new_bpb->oem, "MSWIN4.1", 8);
    new_bpb->bytes_per_sector = 512;
    new_bpb->sectors_per_cluster = 8;
    new_bpb->reserved_sectors = 32;
    new_bpb->num_fats = 2;
    new_bpb->root_entry_count = 0;
    new_bpb->total_sectors_16 = 0;
    new_bpb->media_type = 0xF8;
    new_bpb->sectors_per_fat_16 = 0;
    new_bpb->sectors_per_track = 63;
    new_bpb->num_heads = 255;
    new_bpb->hidden_sectors = fat32_start;
    new_bpb->total_sectors_32 = 2097152;
    new_bpb->sectors_per_fat_32 = 4096;
    new_bpb->flags = 0;
    new_bpb->version = 0;
    new_bpb->root_cluster = 2;
    new_bpb->fs_info_sector = 1;
    new_bpb->backup_boot_sector = 6;
    new_bpb->drive_number = 0x80;
    new_bpb->boot_signature = 0x29;
    new_bpb->volume_id = 0x12345678;
    memcpy(new_bpb->volume_label, "SUNSETOS   ", 11);
    memcpy(new_bpb->fs_type, "FAT32   ", 8);

    boot_sector[510] = 0x55;
    boot_sector[511] = 0xAA;

    if (!write_sector(fat32_start, boot_sector)) {
        vga::print("Failed to write boot sector\n");
        return;
    }

    uint8_t fs_info[SECTOR_SIZE] = {0};
    fs_info[0] = 0x52;
    fs_info[1] = 0x52;
    fs_info[2] = 0x61;
    fs_info[3] = 0x41;
    *(uint32_t*)(fs_info + 484) = 0xFFFFFFFF;
    *(uint32_t*)(fs_info + 488) = 2;
    fs_info[508] = 0x55;
    fs_info[509] = 0xAA;

    if (!write_sector(fat32_start + 1, fs_info)) {
        vga::print("Failed to write FS info\n");
        return;
    }

    uint8_t fat_buffer[SECTOR_SIZE] = {0};
    *(uint32_t*)fat_buffer = 0x0FFFFFF8;
    *(uint32_t*)(fat_buffer + 4) = 0x0FFFFFFF;

    for (uint32_t i = 0; i < new_bpb->sectors_per_fat_32; i++) {
        if (!write_sector(fat32_start + new_bpb->reserved_sectors + i, fat_buffer)) {
            vga::print("Failed to write FAT sector\n");
            return;
        }
    }

    uint8_t root_dir[SECTOR_SIZE] = {0};

    uint32_t root_sector = fat32_start + new_bpb->reserved_sectors + (new_bpb->num_fats * new_bpb->sectors_per_fat_32);
    for (uint32_t i = 0; i < 8; i++) {
        if (!write_sector(root_sector + i, root_dir)) {
            vga::print("Failed to write root directory\n");
            return;
        }
    }

    vga::print("Drive formatted successfully!\n");

    fs_initialized = false;
    use_memory_fs = false;
    init();
}

} // namespace fat32