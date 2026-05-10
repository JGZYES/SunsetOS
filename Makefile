CC := gcc
LD := ld
QEMU := qemu-system-x86_64
GRUB_MKRESCUE := grub-mkrescue
GRUB_INSTALL := grub-install

SRC_DIR := src
BUILD_DIR := build
ISO_DIR := iso
HDD_IMG := sunsetos_hdd.img
MOUNT_DIR := /mnt/sunsetos_hdd
INCLUDE_DIR := $(SRC_DIR)/kernel/include

KERNEL_SOURCES := $(wildcard $(SRC_DIR)/kernel/*.cpp) $(wildcard $(SRC_DIR)/kernel/sqlite/*.cpp)
KERNEL_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(KERNEL_SOURCES))

CFLAGS_KERNEL := -ffreestanding -mno-red-zone -m32 -Wall -Wextra -O2 -fno-exceptions -fno-rtti -I$(INCLUDE_DIR) -nostdinc -fpermissive
LDFLAGS := -T linker.ld -nostdlib -m elf_i386

.PHONY: all run clean install hda run-hda

all: kernel.elf

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/kernel
	mkdir -p $(BUILD_DIR)/kernel/sqlite

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(BUILD_DIR)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

kernel.elf: $(KERNEL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

run: kernel.elf $(HDD_IMG)
	mkdir -p $(ISO_DIR)/boot/grub
	cp kernel.elf $(ISO_DIR)/boot/
	cp grub.cfg $(ISO_DIR)/boot/grub/
	$(GRUB_MKRESCUE) -o sunsetos.iso $(ISO_DIR)
	$(QEMU) -cdrom sunsetos.iso -drive file=$(HDD_IMG),format=raw,if=ide -m 2G

$(HDD_IMG): kernel.elf
	dd if=/dev/zero of=$(HDD_IMG) bs=1M count=100 2>/dev/null
	parted -s $(HDD_IMG) mklabel msdos
	parted -s $(HDD_IMG) mkpart primary fat32 2048s 100%
	parted -s $(HDD_IMG) set 1 boot on
	sudo losetup -f --show -P $(HDD_IMG) > /tmp/loopdev.txt
	sudo mkfs.fat -F 32 $$(cat /tmp/loopdev.txt)p1 -S 512 -s 8 -n SUNSETOS
	sudo mkdir -p $(MOUNT_DIR)
	sudo mount $$(cat /tmp/loopdev.txt)p1 $(MOUNT_DIR)
	sudo mkdir -p $(MOUNT_DIR)/boot/grub
	sudo cp kernel.elf $(MOUNT_DIR)/boot/
	sudo cp grub.cfg $(MOUNT_DIR)/boot/grub/
	sudo $(GRUB_INSTALL) --target=i386-pc --boot-directory=$(MOUNT_DIR)/boot --root-directory=$(MOUNT_DIR) --no-floppy $(HDD_IMG)
	sudo umount $(MOUNT_DIR)
	sudo losetup -d $$(cat /tmp/loopdev.txt)

hda: kernel.elf $(HDD_IMG)
	@echo "Hard disk image created with GRUB and kernel"

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
	sudo rm -rf $(MOUNT_DIR)
