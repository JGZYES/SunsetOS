CC := gcc
LD := ld
QEMU := qemu-system-x86_64
GRUB_MKRESCUE := grub-mkrescue

SRC_DIR := src
BUILD_DIR := build
ISO_DIR := iso
HDD_IMG := sunsetos_hdd.img
INCLUDE_DIR := $(SRC_DIR)/kernel/include

KERNEL_SOURCES := $(wildcard $(SRC_DIR)/kernel/*.cpp) $(wildcard $(SRC_DIR)/kernel/sqlite/*.cpp)
KERNEL_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(KERNEL_SOURCES))

CFLAGS_KERNEL := -ffreestanding -mno-red-zone -m32 -Wall -Wextra -O2 -fno-exceptions -fno-rtti -I$(INCLUDE_DIR) -nostdinc -fpermissive
LDFLAGS := -T linker.ld -nostdlib -m elf_i386

.PHONY: all run clean install hda run-hda

all: kernel.elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkkdir -p $(BUILD_DIR)/kernel
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

$(HDD_IMG): kernel.elf
	dd if=/dev/zero of=$(HDD_IMG) bs=1M count=100 2>/dev/null
	parted -s $(HDD_IMG) mklabel msdos
	parted -s $(HDD_IMG) mkpart primary fat32 2048s 100%
	parted -s $(HDD_IMG) set 1 boot on
	$(QEMU) -drive format=raw,file=$(HDD_IMG),index=0,if=ide -m 2G -nographic &
	sleep 2
	$(QEMU) -drive format=raw,file=$(HDD_IMG),index=0,if=ide -m 2G -nographic -monitor stdio -serial stdio <<EOF
quit
EOF

hda: kernel.elf
	dd if=/dev/zero of=$(HDD_IMG) bs=1M count=100 2>/dev/null || true
	python3 -c "
import struct
with open('$(HDD_IMG)', 'r+b') as f:
    f.seek(0x1BE)
    partitions = [
        0x80, 0x01, 0x01, 0x00, 0x0C, 0xFE, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x55, 0xAA
    ]
    f.write(bytearray(partitions))
"
	qemu-img resize $(HDD_IMG) 100M 2>/dev/null || true
	$(QEMU) -hda $(HDD_IMG) -m 2G &

run-hda: $(HDD_IMG)
	$(QEMU) -hda $(HDD_IMG) -m 2G

install: kernel.elf
	mkdir -p $(ISO_DIR)/boot/grub
	cp kernel.elf $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB_MKRESCUE) -o sunsetos.iso $(ISO_DIR)
	@echo "ISO image created: sunsetos.iso"
	@echo "To install to hard disk, use dd or a virtual machine."

clean:
	rm -rf $(BUILD_DIR) kernel.elf $(ISO_DIR) sunsetos.iso $(HDD_IMG)
