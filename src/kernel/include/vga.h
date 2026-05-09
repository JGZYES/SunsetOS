#pragma once

#include <stdint.h>

namespace vga {

constexpr uint16_t VGA_WIDTH = 80;
constexpr uint16_t VGA_HEIGHT = 25;

enum Color : uint8_t {
    Black = 0,
    Blue = 1,
    Green = 2,
    Cyan = 3,
    Red = 4,
    Magenta = 5,
    Brown = 6,
    LightGray = 7,
    DarkGray = 8,
    LightBlue = 9,
    LightGreen = 10,
    LightCyan = 11,
    LightRed = 12,
    LightMagenta = 13,
    Yellow = 14,
    White = 15
};

inline uint8_t color_code(Color fg, Color bg) {
    return static_cast<uint8_t>(fg) | (static_cast<uint8_t>(bg) << 4);
}

inline uint16_t entry(unsigned char uc, uint8_t color) {
    return static_cast<uint16_t>(uc) | (static_cast<uint16_t>(color) << 8);
}

void init();
void clear();
void set_color(Color fg, Color bg);
void put_char(char c);
void put_string(const char* str);
void put_string(const char* str, Color fg, Color bg);
void print(const char* format, ...);
void newline();
void set_cursor(int x, int y);
void get_cursor(int& x, int& y);

}
