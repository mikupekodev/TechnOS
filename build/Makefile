# ===== パス設定 =====
EDK2_DIR    ?= $(HOME)/edk2
WORKSPACE   := $(shell pwd)
DSC         := TechnOS.dsc
DMG         := bootloader.dmg
MOUNT_POINT := /Volumes/TechnOS

# ===== EDK2設定 =====
ARCH      := X64
TARGET    := DEBUG
TOOLCHAIN := CLANGPDB

# ===== Kernel設定 =====
KERNEL_DIR := ./kernel
KERNEL_ELF := $(KERNEL_DIR)/kernel.elf

NASM := nasm
CC   := x86_64-elf-gcc

CFLAGS := \
    -nostdlib \
    -ffreestanding \
    -mno-red-zone \
    -mcmodel=large \
    -fpic \
    -fpie

LDFLAGS := -pie

# ===== QEMU設定 =====
UTM_QEMU  := /Applications/UTM.app/Contents/Resources/qemu
QEMU      := qemu-system-x86_64
OVMF_CODE := $(UTM_QEMU)/edk2-x86_64-code.fd

QEMUFLAGS := \
    -m 256M \
    -drive format=raw,file=$(DMG) \
    -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
    -serial stdio \
    -net none \
    -monitor telnet::4444,server,nowait \
    -s

.PHONY: all bootloader kernel image run clean

all: bootloader kernel image

# ===== Bootloader (EDK2) =====
bootloader:
	cd $(EDK2_DIR) && \
	. edksetup.sh && \
	build -a $(ARCH) -t $(TOOLCHAIN) -b $(TARGET) \
	-p $(WORKSPACE)/$(DSC)

# ===== Kernel (GCC) =====
kernel: $(KERNEL_ELF)

$(KERNEL_DIR)/start.o: $(KERNEL_DIR)/start.asm
	$(NASM) -f elf64 $< -o $@

$(KERNEL_ELF): $(KERNEL_DIR)/start.o $(KERNEL_DIR)/kernel64.c
	$(CC) $(CFLAGS) \
	    -T $(KERNEL_DIR)/linker.ld \
	    $^ \
	    -o $@

# ===== DMGにコピー =====
image: bootloader kernel
	hdiutil attach $(DMG) -mountpoint $(MOUNT_POINT)
	mkdir -p $(MOUNT_POINT)/EFI/BOOT
	mkdir -p $(MOUNT_POINT)/EFI/technOS/kernel
	cp ./BMPs/logo.bmp $(MOUNT_POINT)/EFI/BOOT/logo.bmp
	cp ./BMPs/back.bmp $(MOUNT_POINT)/EFI/BOOT/back.bmp
	cp ./BMPs/menu.bmp $(MOUNT_POINT)/EFI/BOOT/menu.bmp
	touch $(MOUNT_POINT)/EFI/BOOT/key.bin
	cp $(KERNEL_ELF) $(MOUNT_POINT)/EFI/technOS/kernel/kernel.elf
	cp $(EDK2_DIR)/Build/TechnOS/$(TARGET)_$(TOOLCHAIN)/$(ARCH)/BootLoader.efi \
	    $(MOUNT_POINT)/EFI/BOOT/BOOTX64.EFI
	hdiutil detach $(MOUNT_POINT)

# ===== クリーン =====
clean:
	rm -f $(KERNEL_DIR)/start.o $(KERNEL_ELF)
	cd $(EDK2_DIR) && \
	. edksetup.sh && \
	build -a $(ARCH) -t $(TOOLCHAIN) -b $(TARGET) \
	-p $(EDK2_DIR)/technOS/$(DSC) cleanall

# ===== QEMU実行 =====
run: image
	@osascript -e 'tell application "Terminal" to do script "nc localhost 4444"' &
	$(QEMU) $(QEMUFLAGS)