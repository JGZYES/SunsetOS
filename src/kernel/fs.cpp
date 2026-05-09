#include "fs.h"
#include "vga.h"
#include "libc.h"

namespace fs {

static File files[MAX_FILES];
static char current_path[MAX_PATH] = "/";
static uint32_t root_file = 0;
uint32_t free_file = 0;

void init() {
    memset(files, 0, sizeof(files));
    
    for (size_t i = 0; i < MAX_FILES - 1; i++) {
        files[i].next_file = i + 1;
    }
    files[MAX_FILES - 1].next_file = -1;
    
    strcpy(files[0].name, "/");
    files[0].type = FileType::Directory;
    files[0].size = 0;
    files[0].parent_dir = -1;
    files[0].next_file = -1;
    files[0].first_child = -1;
    root_file = 0;
    strcpy(current_path, "/");
}

static uint32_t allocate_file() {
    if (free_file == -1) return -1;
    uint32_t idx = free_file;
    free_file = files[idx].next_file;
    memset(&files[idx], 0, sizeof(File));
    files[idx].next_file = -1;
    files[idx].first_child = -1;
    files[idx].parent_dir = -1;
    return idx;
}

static void split_path(const char* path, char* dir_part, char* file_part) {
    const char* last_slash = strrchr(path, '/');
    if (last_slash == path) {
        strcpy(dir_part, "/");
        strcpy(file_part, path + 1);
    } else if (last_slash) {
        size_t dir_len = last_slash - path;
        strncpy(dir_part, path, dir_len);
        dir_part[dir_len] = '\0';
        strcpy(file_part, last_slash + 1);
    } else {
        strcpy(dir_part, current_path);
        strcpy(file_part, path);
    }
}

static uint32_t find_file(const char* path, FileType type = FileType::None) {
    if (strcmp(path, "/") == 0) return root_file;
    
    char temp[MAX_PATH];
    strcpy(temp, path);
    
    uint32_t current = root_file;
    char* token = strtok(temp, "/");
    
    while (token) {
        bool found = false;
        uint32_t child_idx = files[current].first_child;
        
        while (child_idx != -1) {
            if (strcmp(files[child_idx].name, token) == 0) {
                if (type == FileType::None || files[child_idx].type == type) {
                    current = child_idx;
                    found = true;
                    break;
                }
            }
            child_idx = files[child_idx].next_file;
        }
        
        if (!found) return -1;
        token = strtok(nullptr, "/");
    }
    
    return current;
}

static uint32_t find_dir(const char* path) {
    return find_file(path, FileType::Directory);
}

int mkdir(const char* path) {
    char dir_part[MAX_PATH];
    char name[MAX_FILENAME];
    split_path(path, dir_part, name);
    
    if (strlen(name) >= MAX_FILENAME) {
        vga::print("Error: filename too long\n");
        return -1;
    }
    
    uint32_t parent_idx = find_dir(dir_part);
    if (parent_idx == -1) {
        vga::print("Error: directory not found\n");
        return -1;
    }
    
    uint32_t child_idx = files[parent_idx].first_child;
    while (child_idx != -1) {
        if (strcmp(files[child_idx].name, name) == 0) {
            vga::print("Error: file exists\n");
            return -1;
        }
        child_idx = files[child_idx].next_file;
    }
    
    uint32_t new_file = allocate_file();
    if (new_file == -1) {
        vga::print("Error: out of space\n");
        return -1;
    }
    
    strcpy(files[new_file].name, name);
    files[new_file].type = FileType::Directory;
    files[new_file].size = 0;
    files[new_file].parent_dir = parent_idx;
    
    files[new_file].next_file = files[parent_idx].first_child;
    files[parent_idx].first_child = new_file;
    
    return 0;
}

int mkfile(const char* path, const char* content) {
    char dir_part[MAX_PATH];
    char name[MAX_FILENAME];
    split_path(path, dir_part, name);
    
    if (strlen(name) >= MAX_FILENAME) {
        vga::print("Error: filename too long\n");
        return -1;
    }
    
    uint32_t parent_idx = find_dir(dir_part);
    if (parent_idx == -1) {
        vga::print("Error: directory not found\n");
        return -1;
    }
    
    uint32_t child_idx = files[parent_idx].first_child;
    while (child_idx != -1) {
        if (strcmp(files[child_idx].name, name) == 0) {
            vga::print("Error: file exists\n");
            return -1;
        }
        child_idx = files[child_idx].next_file;
    }
    
    uint32_t new_file = allocate_file();
    if (new_file == -1) {
        vga::print("Error: out of space\n");
        return -1;
    }
    
    strcpy(files[new_file].name, name);
    files[new_file].type = FileType::File;
    files[new_file].parent_dir = parent_idx;
    
    if (content) {
        size_t len = strlen(content);
        if (len > MAX_FILE_SIZE) len = MAX_FILE_SIZE;
        memcpy(files[new_file].data, content, len);
        files[new_file].size = len;
    } else {
        files[new_file].size = 0;
    }
    
    files[new_file].next_file = files[parent_idx].first_child;
    files[parent_idx].first_child = new_file;
    
    return 0;
}

int ls(const char* path) {
    uint32_t dir_idx = find_dir(path);
    if (dir_idx == -1) {
        vga::print("Error: directory not found\n");
        return -1;
    }
    
    uint32_t child_idx = files[dir_idx].first_child;
    if (child_idx == -1) {
        vga::print("Empty directory\n");
        return 0;
    }
    
    while (child_idx != -1) {
        if (files[child_idx].type == FileType::Directory) {
            vga::put_string("[DIR]  ", vga::Color::LightBlue, vga::Color::Black);
        } else {
            vga::put_string("[FILE] ", vga::Color::LightGreen, vga::Color::Black);
        }
        vga::print("%s\n", files[child_idx].name);
        child_idx = files[child_idx].next_file;
    }
    
    return 0;
}

int fview(const char* path) {
    char dir_part[MAX_PATH];
    char name[MAX_FILENAME];
    split_path(path, dir_part, name);
    
    uint32_t dir_idx = find_dir(dir_part);
    if (dir_idx == -1) {
        vga::print("Error: directory not found\n");
        return -1;
    }
    
    uint32_t child_idx = files[dir_idx].first_child;
    while (child_idx != -1) {
        if (strcmp(files[child_idx].name, name) == 0 && 
            files[child_idx].type == FileType::File) {
            vga::print((const char*)files[child_idx].data);
            vga::print("\n");
            return 0;
        }
        child_idx = files[child_idx].next_file;
    }
    
    vga::print("Error: file not found\n");
    return -1;
}

int fedit(const char* path, const char* content) {
    char dir_part[MAX_PATH];
    char name[MAX_FILENAME];
    split_path(path, dir_part, name);
    
    uint32_t dir_idx = find_dir(dir_part);
    if (dir_idx == -1) {
        vga::print("Error: directory not found\n");
        return -1;
    }
    
    uint32_t child_idx = files[dir_idx].first_child;
    while (child_idx != -1) {
        if (strcmp(files[child_idx].name, name) == 0 && 
            files[child_idx].type == FileType::File) {
            if (content) {
                size_t len = strlen(content);
                if (len > MAX_FILE_SIZE) len = MAX_FILE_SIZE;
                memcpy(files[child_idx].data, content, len);
                files[child_idx].size = len;
            }
            return 0;
        }
        child_idx = files[child_idx].next_file;
    }
    
    vga::print("Error: file not found\n");
    return -1;
}

int remove(const char* path) {
    char dir_part[MAX_PATH];
    char name[MAX_FILENAME];
    split_path(path, dir_part, name);
    
    uint32_t dir_idx = find_dir(dir_part);
    if (dir_idx == -1) {
        vga::print("Error: directory not found\n");
        return -1;
    }
    
    uint32_t child_idx = files[dir_idx].first_child;
    uint32_t prev_idx = -1;
    
    while (child_idx != -1) {
        if (strcmp(files[child_idx].name, name) == 0) {
            if (files[child_idx].type == FileType::Directory) {
                if (files[child_idx].first_child != -1) {
                    vga::print("Error: directory not empty\n");
                    return -1;
                }
            }
            
            if (prev_idx == -1) {
                files[dir_idx].first_child = files[child_idx].next_file;
            } else {
                files[prev_idx].next_file = files[child_idx].next_file;
            }
            
            files[child_idx].next_file = free_file;
            free_file = child_idx;
            return 0;
        }
        prev_idx = child_idx;
        child_idx = files[child_idx].next_file;
    }
    
    vga::print("Error: file not found\n");
    return -1;
}

int rename(const char* old_path, const char* new_name) {
    char dir_part[MAX_PATH];
    char old_name[MAX_FILENAME];
    split_path(old_path, dir_part, old_name);
    
    if (strlen(new_name) >= MAX_FILENAME) {
        vga::print("Error: new filename too long\n");
        return -1;
    }
    
    uint32_t dir_idx = find_dir(dir_part);
    if (dir_idx == -1) {
        vga::print("Error: directory not found\n");
        return -1;
    }
    
    uint32_t child_idx = files[dir_idx].first_child;
    while (child_idx != -1) {
        if (strcmp(files[child_idx].name, new_name) == 0) {
            vga::print("Error: file already exists\n");
            return -1;
        }
        child_idx = files[child_idx].next_file;
    }
    
    child_idx = files[dir_idx].first_child;
    while (child_idx != -1) {
        if (strcmp(files[child_idx].name, old_name) == 0) {
            strcpy(files[child_idx].name, new_name);
            return 0;
        }
        child_idx = files[child_idx].next_file;
    }
    
    vga::print("Error: file not found\n");
    return -1;
}

const char* get_current_dir() {
    return current_path;
}

void set_current_dir(const char* path) {
    uint32_t dir_idx = find_dir(path);
    if (dir_idx != -1) {
        strcpy(current_path, path);
    } else {
        vga::print("Error: directory not found\n");
    }
}

}
