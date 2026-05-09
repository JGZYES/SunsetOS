#pragma once

#include <stdint.h>

namespace syscall {

enum SyscallNumber {
    SYS_PRINT = 0,
    SYS_GET_TIME = 1,
    SYS_GET_PID = 2,
    SYS_EXIT = 3,
    SYS_READ = 4,
    SYS_WRITE = 5,
    SYS_OPEN = 6,
    SYS_CLOSE = 7,
    SYS_MAX
};

void init();
void handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3);

uint32_t print(const char* str);
uint32_t get_time();
uint32_t get_pid();
void exit();

}