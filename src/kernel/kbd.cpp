#include "kbd.h"
#include <stdint.h>

namespace kbd {

static const char scancode_to_ascii[] = {
    0,   0,   '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', 0,   0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', 0,   0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0
};

static const char scancode_to_ascii_shift[] = {
    0,   0,   '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', 0,   0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', 0,   0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0
};

static uint8_t shift_pressed = 0;
static uint8_t capslock_on = 0;

static void io_wait() {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

void init() {
    shift_pressed = 0;
    capslock_on = 0;
}

char read_char() {
    char c = 0;
    
    while (true) {
        uint8_t scancode;
        
        while (true) {
            uint8_t status;
            asm volatile("inb $0x64, %0" : "=a"(status));
            if (status & 1) break;
        }
        
        asm volatile("inb $0x60, %0" : "=a"(scancode));
        
        io_wait();
        
        if (scancode & 0x80) {
            uint8_t key = scancode & 0x7F;
            if (key == 0x2A || key == 0x36) {
                shift_pressed = 0;
            }
            continue;
        }
        
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            continue;
        }
        
        if (scancode == 0x3A) {
            capslock_on = !capslock_on;
            continue;
        }
        
        if (scancode < sizeof(scancode_to_ascii)) {
            char lower = scancode_to_ascii[scancode];
            char upper = scancode_to_ascii_shift[scancode];
            
            if (lower != 0) {
                if ((lower >= 'a' && lower <= 'z') || (lower >= 'A' && lower <= 'Z')) {
                    if (capslock_on && !shift_pressed) {
                        c = upper;
                    } else if (!capslock_on && shift_pressed) {
                        c = upper;
                    } else {
                        c = lower;
                    }
                } else {
                    if (shift_pressed) {
                        c = upper;
                    } else {
                        c = lower;
                    }
                }
                if (c != 0) {
                    break;
                }
            }
        }
        
        if (scancode == 0x1C) {
            c = '\n';
            break;
        }
        
        if (scancode == 0x0E) {
            c = '\b';
            break;
        }
    }
    
    return c;
}

}