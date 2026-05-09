#include "fat32.h"
#include "vga.h"
#include "libc.h"
#include "ata.h"

namespace fat32 {

static BPB bpb;
static uint16_t current_drive = 0;
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
    return bpb.reserved_sectors;
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

static uint32_t get_fat_entry(uint32_t cluster) {
    if (cluster == 0) return 0;
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = get_fat_start_sector() + (fat_offset / SECTOR_SIZE);
    uint32_t offset_in_sector = fat_offset % SECTOR_SIZE;
    
    uint8_t buffer[SECTOR_SIZE];
    ata::read_sector(current_drive, fat_sector, buffer);
    
    return *(uint32_t*)(buffer + offset_in_sector) & 0x0FFFFFFF;
}

static void set_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = get_fat_start_sector() + (fat_offset / SECTOR_SIZE);
    uint32_t offset_in_sector = fat_offset % SECTOR_SIZE;
    
    uint8_t buffer[SECTOR_SIZE];
    ata::read_sector(current_drive, fat_sector, buffer);
    
    *(uint32_t*)(buffer + offset_in_sector) = value & 0x0FFFFFFF;
    
    ata::write_sector(current_drive, fat_sector, buffer);
}

static uint32_t find_free_cluster() {
    uint32_t fat_sectors = bpb.sectors_per_fat_32;
    uint8_t buffer[SECTOR_SIZE];
    
    for (uint32_t sector = get_fat_start_sector(); sector < get_fat_start_sector() + fat_sectors; sector++) {
        ata::read_sector(current_drive, sector, buffer);
        
        for (uint32_t i = 0; i < SECTOR_SIZE; i += 4) {
            uint32_t entry = *(uint32_t*)(buffer + i) & 0x0FFFFFFF;
            if (entry == 0) {
                uint32_t cluster = ((sector - get_fat_start_sector()) * SECTOR_SIZE + i) / 4;
                set_fat_entry(cluster, FAT_EOC);
                return cluster;
            }
        }
    }
    
    return 0;
}

static int read_cluster(uint32_t cluster, uint8_t* buffer) {
    if (cluster < 2) return -1;
    
    uint32_t sector = get_data_start_sector() + (cluster - 2) * get_sectors_per_cluster();
    
    for (uint32_t i = 0; i < get_sectors_per_cluster(); i++) {
        ata::read_sector(current_drive, sector + i, buffer + i * SECTOR_SIZE);
    }
    
    return 0;
}

static int write_cluster(uint32_t cluster, const uint8_t* buffer) {
    if (cluster < 2) return -1;
    
    uint32_t sector = get_data_start_sector() + (cluster - 2) * get_sectors_per_cluster();
    
    for (uint32_t i = 0; i < get_sectors_per_cluster(); i++) {
        ata::write_sector(current_drive, sector + i, buffer + i * SECTOR_SIZE);
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
            short_name[8 + (j - 8)] = toupper(name[i]);
            j++;
        }
        i++;
    }
}

static int find_directory_entry(const char* path, uint32_t* start_cluster, uint32_t* entry_index) {
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
        uint8_t buffer[SECTOR_SIZE * 4];
        uint32_t cluster = current_cluster;
        
        while (cluster != FAT_EOC) {
            if (read_cluster(cluster, buffer) != 0) {
                return -1;
            }
            
            DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer);
            for (int i = 0; i < (int)(get_bytes_per_cluster() / sizeof(DirectoryEntry)); i++) {
                if (entries[i].name[0] == 0x00) break;
                if (entries[i].name[0] == 0xE5) continue;
                
                char entry_name[13];
                short_to_long_name(entries[i].name, entry_name);
                
                if (strcmp(entry_name, token) == 0) {
                    current_cluster = (entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                    token = strtok(nullptr, "/");
                    goto next_token;
                }
            }
            
            cluster = get_fat_entry(cluster);
        }
        
        return -1;
        
next_token:
        continue;
    }
    
    *start_cluster = current_cluster;
    *entry_index = -1;
    return 0;
}

bool init() {
    uint8_t buffer[SECTOR_SIZE];
    
    ata::read_sector(current_drive, 0, buffer);
    
    bool is_all_zero = true;
    for (int i = 0; i < SECTOR_SIZE; i++) {
        if (buffer[i] != 0) {
            is_all_zero = false;
            break;
        }
    }
    
    memcpy(&bpb, buffer, sizeof(BPB));
    
    if (is_all_zero || memcmp(bpb.fs_type, "FAT32   ", 8) != 0) {
        vga::print("No FAT32 filesystem found, using memory filesystem\n");
        use_memory_fs = true;
        fs_initialized = true;
        return true;
    }
    
    vga::print("FAT32 filesystem detected:\n");
    vga::print("  Bytes per sector: %d\n", bpb.bytes_per_sector);
    vga::print("  Sectors per cluster: %d\n", bpb.sectors_per_cluster);
    vga::print("  Reserved sectors: %d\n", bpb.reserved_sectors);
    vga::print("  Number of FATs: %d\n", bpb.num_fats);
    vga::print("  Sectors per FAT: %d\n", bpb.sectors_per_fat_32);
    vga::print("  Root cluster: %d\n", bpb.root_cluster);
    vga::print("  Total sectors: %d\n", get_total_sectors());
    
    fs_initialized = true;
    return true;
}

bool is_ready() {
    return fs_initialized;
}

int list_directory(const char* path, void (*callback)(const char*, uint8_t, uint32_t)) {
    if (!fs_initialized) return -1;
    
    if (use_memory_fs) {
        MemoryFile* file = root_directory;
        while (file) {
            callback(file->name, file->attributes, file->size);
            file = file->next;
        }
        return 0;
    }
    
    uint32_t start_cluster;
    uint32_t entry_index;
    
    if (find_directory_entry(path, &start_cluster, &entry_index) != 0) {
        return -1;
    }
    
    uint8_t buffer[SECTOR_SIZE * 4];
    uint32_t cluster = start_cluster;
    
    while (cluster != FAT_EOC) {
        if (read_cluster(cluster, buffer) != 0) {
            return -1;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer);
        for (int i = 0; i < (int)(get_bytes_per_cluster() / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;
            if ((entries[i].attributes & ATTR_VOLUME_ID) != 0) continue;
            if ((entries[i].attributes & ATTR_HIDDEN) != 0) continue;
            if ((entries[i].attributes & ATTR_SYSTEM) != 0) continue;
            
            char name[13];
            short_to_long_name(entries[i].name, name);
            
            callback(name, entries[i].attributes, entries[i].file_size);
        }
        
        cluster = get_fat_entry(cluster);
    }
    
    return 0;
}

int create_directory(const char* path) {
    if (!fs_initialized) return -1;
    
    if (use_memory_fs) {
        MemoryFile* new_dir = new MemoryFile();
        strcpy(new_dir->name, path);
        new_dir->attributes = ATTR_DIRECTORY;
        new_dir->data = nullptr;
        new_dir->size = 0;
        new_dir->capacity = 0;
        new_dir->next = root_directory;
        root_directory = new_dir;
        return 0;
    }
    
    char path_copy[256];
    strcpy(path_copy, path);
    
    char* dirname = strrchr(path_copy, '/');
    char* parent_path = path_copy;
    if (dirname) {
        *dirname = '\0';
        dirname++;
    } else {
        dirname = path_copy;
        parent_path = "/";
    }
    
    uint32_t parent_cluster;
    uint32_t entry_index;
    
    if (find_directory_entry(parent_path, &parent_cluster, &entry_index) != 0) {
        return -1;
    }
    
    uint32_t cluster = parent_cluster;
    uint8_t buffer[SECTOR_SIZE * 4];
    
    while (cluster != FAT_EOC) {
        if (read_cluster(cluster, buffer) != 0) {
            return -1;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer);
        for (int i = 0; i < (int)(get_bytes_per_cluster() / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                uint32_t new_cluster = find_free_cluster();
                if (new_cluster == 0) {
                    return -1;
                }
                
                long_to_short_name(dirname, entries[i].name);
                entries[i].attributes = ATTR_DIRECTORY;
                entries[i].first_cluster_low = new_cluster & 0xFFFF;
                entries[i].first_cluster_high = (new_cluster >> 16) & 0xFFFF;
                entries[i].file_size = 0;
                
                write_cluster(cluster, buffer);
                
                uint8_t new_dir_buffer[SECTOR_SIZE * 4] = {0};
                write_cluster(new_cluster, new_dir_buffer);
                
                return 0;
            }
        }
        
        uint32_t next_cluster = get_fat_entry(cluster);
        if (next_cluster == FAT_EOC) {
            uint32_t new_cluster = find_free_cluster();
            if (new_cluster == 0) {
                return -1;
            }
            set_fat_entry(cluster, new_cluster);
            memset(buffer, 0, sizeof(buffer));
            write_cluster(new_cluster, buffer);
            cluster = new_cluster;
        } else {
            cluster = next_cluster;
        }
    }
    
    return -1;
}

int create_file(const char* path, uint8_t attributes) {
    if (!fs_initialized) return -1;
    
    if (use_memory_fs) {
        MemoryFile* new_file = new MemoryFile();
        strcpy(new_file->name, path);
        new_file->attributes = attributes;
        new_file->data = nullptr;
        new_file->size = 0;
        new_file->capacity = 0;
        new_file->next = root_directory;
        root_directory = new_file;
        return 0;
    }
    
    char path_copy[256];
    strcpy(path_copy, path);
    
    char* filename = strrchr(path_copy, '/');
    char* dir_path = path_copy;
    if (filename) {
        *filename = '\0';
        filename++;
    } else {
        filename = path_copy;
        dir_path = "/";
    }
    
    uint32_t dir_cluster;
    uint32_t entry_index;
    
    if (find_directory_entry(dir_path, &dir_cluster, &entry_index) != 0) {
        return -1;
    }
    
    uint32_t cluster = dir_cluster;
    uint8_t buffer[SECTOR_SIZE * 4];
    
    while (cluster != FAT_EOC) {
        if (read_cluster(cluster, buffer) != 0) {
            return -1;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer);
        for (int i = 0; i < (int)(get_bytes_per_cluster() / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                uint32_t new_cluster = find_free_cluster();
                if (new_cluster == 0) {
                    return -1;
                }
                
                long_to_short_name(filename, entries[i].name);
                entries[i].attributes = attributes;
                entries[i].first_cluster_low = new_cluster & 0xFFFF;
                entries[i].first_cluster_high = (new_cluster >> 16) & 0xFFFF;
                entries[i].file_size = 0;
                
                write_cluster(cluster, buffer);
                return 0;
            }
        }
        
        uint32_t next_cluster = get_fat_entry(cluster);
        if (next_cluster == FAT_EOC) {
            uint32_t new_cluster = find_free_cluster();
            if (new_cluster == 0) {
                return -1;
            }
            set_fat_entry(cluster, new_cluster);
            memset(buffer, 0, sizeof(buffer));
            write_cluster(new_cluster, buffer);
            cluster = new_cluster;
        } else {
            cluster = next_cluster;
        }
    }
    
    return -1;
}

int delete_file(const char* path) {
    if (!fs_initialized) return -1;
    
    if (use_memory_fs) {
        MemoryFile** ptr = &root_directory;
        while (*ptr) {
            if (strcmp((*ptr)->name, path) == 0) {
                MemoryFile* temp = *ptr;
                *ptr = (*ptr)->next;
                delete[] temp->data;
                delete temp;
                return 0;
            }
            ptr = &(*ptr)->next;
        }
        return -1;
    }
    
    char path_copy[256];
    strcpy(path_copy, path);
    
    char* filename = strrchr(path_copy, '/');
    char* dir_path = path_copy;
    if (filename) {
        *filename = '\0';
        filename++;
    } else {
        filename = path_copy;
        dir_path = "/";
    }
    
    uint32_t dir_cluster;
    uint32_t entry_index;
    
    if (find_directory_entry(dir_path, &dir_cluster, &entry_index) != 0) {
        return -1;
    }
    
    uint32_t cluster = dir_cluster;
    uint8_t buffer[SECTOR_SIZE * 4];
    
    while (cluster != FAT_EOC) {
        if (read_cluster(cluster, buffer) != 0) {
            return -1;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(buffer);
        for (int i = 0; i < (int)(get_bytes_per_cluster() / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) break;
            
            char entry_name[13];
            short_to_long_name(entries[i].name, entry_name);
            
            if (strcmp(entry_name, filename) == 0) {
                uint32_t file_cluster = (entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                
                while (file_cluster != FAT_EOC) {
                    uint32_t next_cluster = get_fat_entry(file_cluster);
                    set_fat_entry(file_cluster, 0);
                    file_cluster = next_cluster;
                }
                
                entries[i].name[0] = 0xE5;
                write_cluster(cluster, buffer);
                return 0;
            }
        }
        
        cluster = get_fat_entry(cluster);
    }
    
    return -1;
}

int read_file(const char* path, uint8_t* buffer, uint32_t max_size, uint32_t* bytes_read) {
    if (!fs_initialized) return -1;
    
    *bytes_read = 0;
    
    if (use_memory_fs) {
        MemoryFile* file = root_directory;
        while (file) {
            if (strcmp(file->name, path) == 0) {
                *bytes_read = file->size < max_size ? file->size : max_size;
                memcpy(buffer, file->data, *bytes_read);
                return 0;
            }
            file = file->next;
        }
        return -1;
    }
    
    char path_copy[256];
    strcpy(path_copy, path);
    
    char* filename = strrchr(path_copy, '/');
    char* dir_path = path_copy;
    if (filename) {
        *filename = '\0';
        filename++;
    } else {
        filename = path_copy;
        dir_path = "/";
    }
    
    uint32_t dir_cluster;
    uint32_t entry_index;
    
    if (find_directory_entry(dir_path, &dir_cluster, &entry_index) != 0) {
        return -1;
    }
    
    uint32_t cluster = dir_cluster;
    uint8_t dir_buffer[SECTOR_SIZE * 4];
    
    while (cluster != FAT_EOC) {
        if (read_cluster(cluster, dir_buffer) != 0) {
            return -1;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(dir_buffer);
        for (int i = 0; i < (int)(get_bytes_per_cluster() / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) break;
            
            char entry_name[13];
            short_to_long_name(entries[i].name, entry_name);
            
            if (strcmp(entry_name, filename) == 0) {
                uint32_t file_cluster = (entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
                uint32_t remaining = entries[i].file_size;
                uint32_t offset = 0;
                
                while (file_cluster != FAT_EOC && remaining > 0 && offset < max_size) {
                    uint8_t cluster_buffer[SECTOR_SIZE * 4];
                    if (read_cluster(file_cluster, cluster_buffer) != 0) {
                        return -1;
                    }
                    
                    uint32_t to_copy = get_bytes_per_cluster();
                    if (to_copy > remaining) to_copy = remaining;
                    if (to_copy > max_size - offset) to_copy = max_size - offset;
                    
                    memcpy(buffer + offset, cluster_buffer, to_copy);
                    offset += to_copy;
                    remaining -= to_copy;
                    
                    file_cluster = get_fat_entry(file_cluster);
                }
                
                *bytes_read = offset;
                return 0;
            }
        }
        
        cluster = get_fat_entry(cluster);
    }
    
    return -1;
}

int write_file(const char* path, const uint8_t* data, uint32_t size) {
    if (!fs_initialized) return -1;
    
    if (use_memory_fs) {
        MemoryFile* file = root_directory;
        while (file) {
            if (strcmp(file->name, path) == 0) {
                if (size > file->capacity) {
                    delete[] file->data;
                    file->data = new uint8_t[size];
                    file->capacity = size;
                }
                memcpy(file->data, data, size);
                file->size = size;
                return 0;
            }
            file = file->next;
        }
        
        create_file(path, ATTR_ARCHIVE);
        return write_file(path, data, size);
    }
    
    char path_copy[256];
    strcpy(path_copy, path);
    
    char* filename = strrchr(path_copy, '/');
    char* dir_path = path_copy;
    if (filename) {
        *filename = '\0';
        filename++;
    } else {
        filename = path_copy;
        dir_path = "/";
    }
    
    uint32_t dir_cluster;
    uint32_t entry_index;
    
    if (find_directory_entry(dir_path, &dir_cluster, &entry_index) != 0) {
        return -1;
    }
    
    uint32_t cluster = dir_cluster;
    uint8_t dir_buffer[SECTOR_SIZE * 4];
    DirectoryEntry* found_entry = nullptr;
    uint32_t found_cluster;
    
    while (cluster != FAT_EOC) {
        if (read_cluster(cluster, dir_buffer) != 0) {
            return -1;
        }
        
        DirectoryEntry* entries = reinterpret_cast<DirectoryEntry*>(dir_buffer);
        for (int i = 0; i < (int)(get_bytes_per_cluster() / sizeof(DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) break;
            
            char entry_name[13];
            short_to_long_name(entries[i].name, entry_name);
            
            if (strcmp(entry_name, filename) == 0) {
                found_entry = &entries[i];
                found_cluster = cluster;
                goto found;
            }
        }
        
        cluster = get_fat_entry(cluster);
    }
    
    return -1;
    
found:
    uint32_t file_cluster = (found_entry->first_cluster_high << 16) | found_entry->first_cluster_low;
    uint32_t remaining = size;
    uint32_t offset = 0;
    uint32_t current_cluster = file_cluster;
    
    while (remaining > 0) {
        uint8_t cluster_buffer[SECTOR_SIZE * 4];
        memset(cluster_buffer, 0, sizeof(cluster_buffer));
        
        uint32_t to_write = get_bytes_per_cluster();
        if (to_write > remaining) to_write = remaining;
        
        memcpy(cluster_buffer, data + offset, to_write);
        
        if (write_cluster(current_cluster, cluster_buffer) != 0) {
            return -1;
        }
        
        offset += to_write;
        remaining -= to_write;
        
        if (remaining > 0) {
            uint32_t next_cluster = get_fat_entry(current_cluster);
            if (next_cluster == FAT_EOC) {
                uint32_t new_cluster = find_free_cluster();
                if (new_cluster == 0) {
                    return -1;
                }
                set_fat_entry(current_cluster, new_cluster);
                next_cluster = new_cluster;
            }
            current_cluster = next_cluster;
        }
    }
    
    found_entry->file_size = size;
    write_cluster(found_cluster, dir_buffer);
    
    return 0;
}

const char* get_current_dir() {
    return current_path;
}

void set_current_dir(const char* path) {
    strncpy(current_path, path, sizeof(current_path) - 1);
}

void print_filesystem_info() {
    if (!fs_initialized) {
        vga::print("Filesystem not initialized\n");
        return;
    }
    
    if (use_memory_fs) {
        vga::print("Using memory filesystem\n");
        vga::print("Current directory: %s\n", current_path);
        
        int count = 0;
        MemoryFile* file = root_directory;
        while (file) {
            count++;
            file = file->next;
        }
        vga::print("Number of files: %d\n", count);
    } else {
        vga::print("FAT32 Filesystem Info:\n");
        vga::print("  Bytes per sector: %d\n", bpb.bytes_per_sector);
        vga::print("  Sectors per cluster: %d\n", bpb.sectors_per_cluster);
        vga::print("  Bytes per cluster: %d\n", get_bytes_per_cluster());
        vga::print("  Number of FATs: %d\n", bpb.num_fats);
        vga::print("  Sectors per FAT: %d\n", bpb.sectors_per_fat_32);
        vga::print("  Root cluster: %d\n", bpb.root_cluster);
        vga::print("  Total sectors: %d\n", get_total_sectors());
        vga::print("  Total clusters: %d\n", (get_total_sectors() - get_data_start_sector()) / get_sectors_per_cluster());
        vga::print("  Current directory: %s\n", current_path);
    }
}

void format_drive(uint16_t drive) {
    vga::print("Formatting drive %d as FAT32...\n", drive);
    
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
    new_bpb->hidden_sectors = 0;
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
    
    ata::write_sector(drive, 0, boot_sector);
    
    uint8_t fs_info[SECTOR_SIZE] = {0};
    fs_info[0] = 0x52;
    fs_info[1] = 0x52;
    fs_info[2] = 0x61;
    fs_info[3] = 0x41;
    *(uint32_t*)(fs_info + 484) = 0xFFFFFFFF;
    *(uint32_t*)(fs_info + 488) = 2;
    fs_info[508] = 0x55;
    fs_info[509] = 0xAA;
    
    ata::write_sector(drive, 1, fs_info);
    
    uint8_t fat_buffer[SECTOR_SIZE] = {0};
    *(uint32_t*)fat_buffer = 0x0FFFFFF8;
    *(uint32_t*)(fat_buffer + 4) = 0x0FFFFFFF;
    
    for (uint32_t i = 0; i < new_bpb->sectors_per_fat_32; i++) {
        ata::write_sector(drive, new_bpb->reserved_sectors + i, fat_buffer);
    }
    
    uint8_t root_dir[SECTOR_SIZE * 8] = {0};
    
    uint32_t root_sector = new_bpb->reserved_sectors + (new_bpb->num_fats * new_bpb->sectors_per_fat_32);
    for (uint32_t i = 0; i < 8; i++) {
        ata::write_sector(drive, root_sector + i, root_dir + i * SECTOR_SIZE);
    }
    
    vga::print("Drive formatted successfully!\n");
    
    fs_initialized = false;
    use_memory_fs = false;
    init();
}

}
