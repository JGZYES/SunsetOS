#include "sqlite_os_ext.h"
#include "libc.h"

namespace sqlite_os {

constexpr size_t HEAP_SIZE = 256 * 1024;
static uint8_t heap[HEAP_SIZE];
static size_t heap_used = 0;

constexpr size_t MAX_FILE_HANDLES = 16;
static FileHandle file_handles[MAX_FILE_HANDLES];

void* malloc(size_t size) {
    if (size == 0) return nullptr;

    size = (size + 7) & ~7;

    if (heap_used + size > HEAP_SIZE) {
        return nullptr;
    }

    void* ptr = &heap[heap_used];
    heap_used += size;
    return ptr;
}

void free(void* ptr) {
    (void)ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return nullptr;
    }

    void* new_ptr = malloc(new_size);
    if (new_ptr && ptr) {
        memcpy(new_ptr, ptr, new_size);
        free(ptr);
    }
    return new_ptr;
}

void* memcpy(void* dest, const void* src, size_t num) {
    return ::memcpy(dest, src, num);
}

void* memset(void* ptr, int value, size_t num) {
    return ::memset(ptr, value, num);
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* p1 = (const unsigned char*)ptr1;
    const unsigned char* p2 = (const unsigned char*)ptr2;
    for (size_t i = 0; i < num; i++) {
        if (p1[i] < p2[i]) return -1;
        if (p1[i] > p2[i]) return 1;
    }
    return 0;
}

size_t strlen(const char* str) {
    return ::strlen(str);
}

char* strcpy(char* dest, const char* src) {
    return ::strcpy(dest, src);
}

char* strncpy(char* dest, const char* src, size_t num) {
    return ::strncpy(dest, src, num);
}

int strcmp(const char* str1, const char* str2) {
    return ::strcmp(str1, str2);
}

int strncmp(const char* str1, const char* str2, size_t num) {
    while (num-- > 0) {
        if (*str1 != *str2) {
            return *(unsigned char*)str1 - *(unsigned char*)str2;
        }
        if (*str1 == '\0') return 0;
        str1++;
        str2++;
    }
    return 0;
}

char* strchr(const char* str, int c) {
    return ::strchr(str, c);
}

char* strcat(char* dest, const char* src) {
    return ::strcat(dest, src);
}

int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);

    int i = 0;
    const char* p = format;
    char buf[32];

    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    int len = 0;
                    if (val == 0) {
                        buf[len++] = '0';
                    } else {
                        int temp = val;
                        char tmpbuf[16];
                        while (temp > 0) {
                            tmpbuf[len++] = '0' + (temp % 10);
                            temp /= 10;
                        }
                        for (int j = len - 1; j >= 0; j--) {
                            buf[len - j - 1] = tmpbuf[j];
                        }
                    }
                    for (int j = 0; j < len; j++) {
                        str[i++] = buf[j];
                    }
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    while (*s) {
                        str[i++] = *s++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    str[i++] = c;
                    break;
                }
                case 'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    int len = 0;
                    if (val == 0) {
                        buf[len++] = '0';
                    } else {
                        char tmpbuf[16];
                        while (val > 0) {
                            tmpbuf[len++] = '0' + (val % 10);
                            val /= 10;
                        }
                        for (int j = len - 1; j >= 0; j--) {
                            buf[len - j - 1] = tmpbuf[j];
                        }
                    }
                    for (int j = 0; j < len; j++) {
                        str[i++] = buf[j];
                    }
                    break;
                }
                case 'x': {
                    unsigned int val = va_arg(args, unsigned int);
                    int len = 0;
                    char hexchars[] = "0123456789abcdef";
                    if (val == 0) {
                        buf[len++] = '0';
                    } else {
                        char tmpbuf[16];
                        while (val > 0) {
                            tmpbuf[len++] = hexchars[val % 16];
                            val /= 16;
                        }
                        for (int j = len - 1; j >= 0; j--) {
                            buf[len - j - 1] = tmpbuf[j];
                        }
                    }
                    for (int j = 0; j < len; j++) {
                        str[i++] = buf[j];
                    }
                    break;
                }
                default:
                    str[i++] = *p;
                    break;
            }
        } else {
            str[i++] = *p;
        }
        p++;
    }
    str[i] = '\0';
    va_end(args);
    return i;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    if (size == 0) return 0;

    va_list args;
    va_start(args, format);

    int i = 0;
    const char* p = format;

    while (*p && i < (int)size - 1) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    char buf[32];
                    int len = 0;
                    if (val == 0) {
                        buf[len++] = '0';
                    } else {
                        char tmpbuf[16];
                        while (val > 0) {
                            tmpbuf[len++] = '0' + (val % 10);
                            val /= 10;
                        }
                        for (int j = len - 1; j >= 0; j--) {
                            buf[len - j - 1] = tmpbuf[j];
                        }
                    }
                    for (int j = 0; j < len && i < (int)size - 1; j++) {
                        str[i++] = buf[j];
                    }
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    while (*s && i < (int)size - 1) {
                        str[i++] = *s++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    if (i < (int)size - 1) {
                        str[i++] = c;
                    }
                    break;
                }
                default:
                    if (i < (int)size - 1) {
                        str[i++] = *p;
                    }
                    break;
            }
        } else {
            str[i++] = *p;
        }
        p++;
    }
    str[i] = '\0';
    va_end(args);
    return i;
}

FileHandle* file_open(const char* path, const char* mode) {
    (void)mode;
    (void)path;

    for (size_t i = 0; i < MAX_FILE_HANDLES; i++) {
        if (!file_handles[i].used) {
            file_handles[i].used = true;
            file_handles[i].position = 0;
            file_handles[i].index = i;
            return &file_handles[i];
        }
    }
    return nullptr;
}

int file_read(FileHandle* fh, void* buf, size_t size) {
    if (!fh || !fh->used) return -1;
    (void)buf;
    (void)size;
    return 0;
}

int file_write(FileHandle* fh, const void* buf, size_t size) {
    if (!fh || !fh->used) return -1;
    (void)buf;
    (void)size;
    return 0;
}

int file_seek(FileHandle* fh, long offset, int whence) {
    if (!fh || !fh->used) return -1;

    if (whence == 0) {
        fh->position = offset;
    } else if (whence == 1) {
        fh->position += offset;
    } else if (whence == 2) {
        fh->position = offset;
    }
    return 0;
}

long file_tell(FileHandle* fh) {
    if (!fh || !fh->used) return -1;
    return fh->position;
}

int file_close(FileHandle* fh) {
    if (!fh || !fh->used) return -1;
    fh->used = false;
    return 0;
}

int file_exists(const char* path) {
    (void)path;
    return 0;
}

long file_size(const char* path) {
    return 0;
}

}