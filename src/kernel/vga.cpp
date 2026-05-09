#include "vga.h"
#include <stdarg.h>
#include <stddef.h>

namespace vga {

static uint16_t* const VGA_BUFFER = reinterpret_cast<uint16_t*>(0xB8000);

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = color_code(Color::LightGray, Color::Black);

static char* itoa(int num, char* str, int base) {
    int i = 0;
    bool is_negative = false;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0 && base == 10) {
        is_negative = true;
        num = -num;
    }

    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    if (is_negative)
        str[i++] = '-';

    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}

void init() {
    clear();
}

void clear() {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int index = y * VGA_WIDTH + x;
            VGA_BUFFER[index] = entry(' ', current_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    set_cursor(0, 0);
}

void set_color(Color fg, Color bg) {
    current_color = color_code(fg, bg);
}

void put_char(char c) {
    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_x = VGA_WIDTH - 1;
            cursor_y--;
        }
        const int index = cursor_y * VGA_WIDTH + cursor_x;
        VGA_BUFFER[index] = entry(' ', current_color);
        set_cursor(cursor_x, cursor_y);
        return;
    }
    
    if (c == '\n') {
        newline();
        return;
    }
    
    if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            put_char(' ');
        }
        return;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        newline();
    }
    
    const int index = cursor_y * VGA_WIDTH + cursor_x;
    VGA_BUFFER[index] = entry(c, current_color);
    cursor_x++;
    set_cursor(cursor_x, cursor_y);
}

void put_string(const char* str) {
    while (*str) {
        put_char(*str++);
    }
}

void put_string(const char* str, Color fg, Color bg) {
    uint8_t old_color = current_color;
    set_color(fg, bg);
    put_string(str);
    set_color(Color::LightGray, Color::Black);
}

void newline() {
    cursor_x = 0;
    cursor_y++;
    
    if (cursor_y >= VGA_HEIGHT) {
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                const int src_index = y * VGA_WIDTH + x;
                const int dst_index = (y - 1) * VGA_WIDTH + x;
                VGA_BUFFER[dst_index] = VGA_BUFFER[src_index];
            }
        }
        
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
            VGA_BUFFER[index] = entry(' ', current_color);
        }
        
        cursor_y = VGA_HEIGHT - 1;
    }
    
    set_cursor(cursor_x, cursor_y);
}

void set_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    uint8_t pos_low = static_cast<uint8_t>(pos);
    uint8_t pos_high = static_cast<uint8_t>(pos >> 8);
    
    asm volatile("outb %b0, %w1" : : "a"(0x0F), "Nd"(0x3D4));
    asm volatile("outb %b0, %w1" : : "a"(pos_low), "Nd"(0x3D5));
    asm volatile("outb %b0, %w1" : : "a"(0x0E), "Nd"(0x3D4));
    asm volatile("outb %b0, %w1" : : "a"(pos_high), "Nd"(0x3D5));
}

void get_cursor(int& x, int& y) {
    x = cursor_x;
    y = cursor_y;
}

void print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'c': {
                    char c = va_arg(args, int);
                    put_char(c);
                    break;
                }
                case 's': {
                    const char* str = va_arg(args, const char*);
                    put_string(str);
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    char buffer[20];
                    itoa(num, buffer, 10);
                    put_string(buffer);
                    break;
                }
                case 'x': {
                    int num = va_arg(args, int);
                    char buffer[20];
                    itoa(num, buffer, 16);
                    put_string(buffer);
                    break;
                }
                default:
                    put_char(*format);
                    break;
            }
        } else {
            put_char(*format);
        }
        format++;
    }
    
    va_end(args);
}

}
