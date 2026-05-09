#pragma once

#include <stdint.h>

namespace elf {

constexpr uint8_t ELF_MAGIC[4] = {0x7F, 'E', 'L', 'F'};

constexpr uint32_t USER_BASE = 0x80000000;
constexpr uint32_t USER_STACK_TOP = 0x90000000;
constexpr uint32_t USER_STACK_SIZE = 0x100000;

enum class ELFClass {
    ELF32 = 1,
    ELF64 = 2
};

enum class ELFData {
    LSB = 1,
    MSB = 2
};

enum class ELFType {
    NONE = 0,
    REL = 1,
    EXEC = 2,
    DYN = 3,
    CORE = 4
};

enum class ELFMachine {
    NONE = 0,
    I386 = 3,
    X86_64 = 62
};

struct Elf32Header {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct Elf32ProgramHeader {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed));

struct Elf32SectionHeader {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} __attribute__((packed));

constexpr uint32_t PT_NULL = 0;
constexpr uint32_t PT_LOAD = 1;
constexpr uint32_t PT_DYNAMIC = 2;
constexpr uint32_t PT_INTERP = 3;
constexpr uint32_t PT_NOTE = 4;
constexpr uint32_t PT_SHLIB = 5;
constexpr uint32_t PT_PHDR = 6;

constexpr uint32_t PF_R = 4;
constexpr uint32_t PF_W = 2;
constexpr uint32_t PF_X = 1;

bool is_valid_elf(const uint8_t* data);
bool parse_elf_header(const uint8_t* data, Elf32Header* header);
int load_elf_safe(const uint8_t* data, uint32_t size, uint32_t* entry_point);

}
