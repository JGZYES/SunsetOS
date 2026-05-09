#pragma once

#include <stdint.h>
#include <stddef.h>

namespace fs {

constexpr size_t MAX_FILES = 128;
constexpr size_t MAX_FILENAME = 12;
constexpr size_t MAX_FILE_SIZE = 4096;
constexpr size_t MAX_PATH = 256;

enum class FileType {
    None,
    File,
    Directory
};

struct File {
    char name[MAX_FILENAME];
    FileType type;
    uint32_t size;
    uint8_t data[MAX_FILE_SIZE];
    uint32_t parent_dir;
    uint32_t next_file;
    uint32_t first_child;
};

extern uint32_t free_file;

void init();
int mkdir(const char* path);
int mkfile(const char* path, const char* content = "");
int ls(const char* path);
int fview(const char* path);
int fedit(const char* path, const char* content);
int remove(const char* path);
int rename(const char* old_path, const char* new_name);
const char* get_current_dir();
void set_current_dir(const char* path);

}
