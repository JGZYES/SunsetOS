CC := gcc
LD := ld
QEMU := qemu-system-x86_64
GRUB_MKRESCUE := grub-mkrescue

SRC_DIR := src
BUILD_DIR := build
ISO_DIR := iso
INCLUDE_DIR := $(SRC_DIR)/kernel/include

KERNEL_SOURCES := $(wildcard $(SRC_DIR)/kernel/*.cpp) $(wildcard $(SRC_DIR)/kernel/sqlite/*.cpp)
KERNEL_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(KERNEL_SOURCES))

CFLAGS_KERNEL := -ffreestanding -mno-red-zone -m32 -Wall -Wextra -O2 -fno-exceptions -fno-rtti -I$(INCLUDE_DIR) -nostdinc -fpermissive
LDFLAGS := -T linker.ld -nostdlib -m elf_i386

.PHONY: all run clean install

all: kernel.elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/kernel
	mkdir -p $(BUILD_DIR)/kernel/sqlite

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(BUILD_DIR)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

kernel.elf: $(KERNEL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

run: kernel.elf
	mkdir -p $(ISO_DIR)/boot/grub
	cp kernel.elf $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB_MKRESCUE) -o sunsetos.iso $(ISO_DIR)
	$(QEMU) -cdrom sunsetos.iso -m 2G

install: kernel.elf
	mkdir -p $(ISO_DIR)/boot/grub
	cp kernel.elf $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB_MKRESCUE) -o sunsetos.iso $(ISO_DIR)
	@echo "ISO image created: sunsetos.iso"
	@echo "To install to hard disk, use dd or a virtual machine."

clean:
	rm -rf $(BUILD_DIR) kernel.elf $(ISO_DIR) sunsetos.iso
