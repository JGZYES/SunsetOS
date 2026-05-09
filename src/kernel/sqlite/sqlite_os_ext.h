#ifndef SQLITE_OS_EXT_H
#define SQLITE_OS_EXT_H

#include <stdint.h>
#include <stddef.h>

namespace sqlite_os {

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t new_size);
void* memcpy(void* dest, const void* src, size_t num);
void* memset(void* ptr, int value, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t num);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t num);
char* strchr(const char* str, int c);
char* strcat(char* dest, const char* src);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);

struct FileHandle {
    uint32_t index;
    uint32_t position;
    bool used;
};

FileHandle* file_open(const char* path, const char* mode);
int file_read(FileHandle* fh, void* buf, size_t size);
int file_write(FileHandle* fh, const void* buf, size_t size);
int file_seek(FileHandle* fh, long offset, int whence);
long file_tell(FileHandle* fh);
int file_close(FileHandle* fh);
int file_exists(const char* path);
long file_size(const char* path);

}

#endif