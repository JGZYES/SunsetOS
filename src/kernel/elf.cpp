#include "elf.h"
#include "vga.h"
#include "libc.h"

namespace elf {

bool is_valid_elf(const uint8_t* data) {
    if (data == nullptr) return false;
    
    for (int i = 0; i < 4; i++) {
        if (data[i] != ELF_MAGIC[i]) {
            return false;
        }
    }
    
    return true;
}

bool parse_elf_header(const uint8_t* data, Elf32Header* header) {
    if (!is_valid_elf(data)) {
        vga::print("Error: Not a valid ELF file\n");
        return false;
    }
    
    memcpy(header, data, sizeof(Elf32Header));
    
    if (header->e_ident[4] != static_cast<uint8_t>(ELFClass::ELF32)) {
        vga::print("Error: Only 32-bit ELF files are supported\n");
        return false;
    }
    
    if (header->e_ident[5] != static_cast<uint8_t>(ELFData::LSB)) {
        vga::print("Error: Only little-endian ELF files are supported\n");
        return false;
    }
    
    if (header->e_type != static_cast<uint16_t>(ELFType::EXEC)) {
        vga::print("Error: Only executable ELF files are supported\n");
        return false;
    }
    
    if (header->e_machine != static_cast<uint16_t>(ELFMachine::I386)) {
        vga::print("Error: Only i386 ELF files are supported\n");
        return false;
    }
    
    return true;
}

int load_elf_safe(const uint8_t* data, uint32_t size, uint32_t* entry_point) {
    Elf32Header header;
    if (!parse_elf_header(data, &header)) {
        return -1;
    }
    
    vga::print("ELF file loaded:\n");
    vga::print("  Entry point: 0x%X\n", header.e_entry);
    vga::print("  Program headers: %d\n", header.e_phnum);
    vga::print("  Program header offset: 0x%X\n", header.e_phoff);
    
    uint32_t max_vaddr = 0;
    
    for (uint16_t i = 0; i < header.e_phnum; i++) {
        uint32_t ph_offset = header.e_phoff + (i * header.e_phentsize);
        
        if (ph_offset + sizeof(Elf32ProgramHeader) > size) {
            vga::print("Error: Program header out of bounds\n");
            return -1;
        }
        
        Elf32ProgramHeader ph;
        memcpy(&ph, data + ph_offset, sizeof(Elf32ProgramHeader));
        
        if (ph.p_type == PT_LOAD) {
            vga::print("  Segment %d:\n", i);
            vga::print("    Type: LOAD\n");
            vga::print("    Offset: 0x%X\n", ph.p_offset);
            vga::print("    Virtual address: 0x%X\n", ph.p_vaddr);
            vga::print("    Physical address: 0x%X\n", ph.p_paddr);
            vga::print("    File size: 0x%X\n", ph.p_filesz);
            vga::print("    Memory size: 0x%X\n", ph.p_memsz);
            vga::print("    Flags: ");
            if (ph.p_flags & PF_R) vga::print("R");
            if (ph.p_flags & PF_W) vga::print("W");
            if (ph.p_flags & PF_X) vga::print("X");
            vga::print("\n");
            
            if (ph.p_offset + ph.p_filesz > size) {
                vga::print("Error: Segment data out of bounds\n");
                return -1;
            }
            
            uint32_t dest_vaddr = ph.p_vaddr;
            
            if (dest_vaddr < USER_BASE) {
                vga::print("Warning: Relocating segment from 0x%X to user space\n", dest_vaddr);
                dest_vaddr = USER_BASE + (dest_vaddr & 0x0FFFFFFF);
            }
            
            if (dest_vaddr + ph.p_memsz > USER_STACK_TOP) {
                vga::print("Error: Segment exceeds user space limit\n");
                return -1;
            }
            
            if (dest_vaddr + ph.p_memsz > max_vaddr) {
                max_vaddr = dest_vaddr + ph.p_memsz;
            }
            
            uint8_t* dest = reinterpret_cast<uint8_t*>(dest_vaddr);
            
            vga::print("    Loading to: 0x%X\n", dest_vaddr);
            
            memcpy(dest, data + ph.p_offset, ph.p_filesz);
            
            if (ph.p_memsz > ph.p_filesz) {
                memset(dest + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
            }
        }
    }
    
    uint32_t actual_entry = header.e_entry;
    if (actual_entry < USER_BASE) {
        actual_entry = USER_BASE + (actual_entry & 0x0FFFFFFF);
    }
    
    *entry_point = actual_entry;
    vga::print("ELF file loaded successfully, entry at 0x%X\n", *entry_point);
    
    vga::print("Initializing user stack at 0x%X...\n", USER_STACK_TOP);
    memset(reinterpret_cast<void*>(USER_STACK_TOP - USER_STACK_SIZE), 0, USER_STACK_SIZE);
    
    return 0;
}

}
