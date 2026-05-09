#include <stdint.h>

#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_HEADER_FLAGS 0x00000003
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

extern "C" {

asm(
    ".section .multiboot\n"
    "    .long 0x1BADB002\n"
    "    .long 0x00000003\n"
    "    .long -(0x1BADB002 + 0x00000003)\n"
    ".section .text\n"
    ".globl _start\n"
    "_start:\n"
    "    cli\n"
    "    movl %esp, %ebp\n"
    "    push %ebx\n"
    "    push %eax\n"
    "    call kernel_main\n"
    "hang:\n"
    "    hlt\n"
    "    jmp hang\n"
);

void kernel_main(uint32_t magic, uint32_t info);

}

#include "shell.h"

extern "C" void kernel_main(uint32_t magic, uint32_t info) {
    if (magic != 0x2BADB002) {
        while (1);
    }

    shell::init();
    shell::run();

    while (1) {
        asm volatile("hlt");
    }
}
