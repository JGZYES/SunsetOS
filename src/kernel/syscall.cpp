#include "syscall.h"
#include "vga.h"
#include "libc.h"

namespace syscall {

static uint32_t pid_counter = 1;

void init() {
    vga::print("System call table initialized\n");
}

uint32_t print(const char* str) {
    vga::print(str);
    return 0;
}

uint32_t get_time() {
    uint32_t lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo) : : "edx");
    return lo;
}

uint32_t get_pid() {
    return pid_counter++;
}

void exit() {
    while (1) __asm__ __volatile__ ("hlt");
}

}