PROJECT := avos
ISO_LABEL := AVOS

BUILD_DIR := build
APP_STAGING_DIR := $(BUILD_DIR)/app-root
BOOTLOADER_DIR := Bootloader
BOOT_BUILD_DIR := $(BOOTLOADER_DIR)/build
KERNEL_DIR := Kernel
KERNEL_BUILD_DIR := $(KERNEL_DIR)/build
ROOTFS_DIR := Rootfs
LIBC_DIR := Libc
ADK_DIR := ADK
AETHER_DIR := Aether
SRC_APPS_DIR := Src
GUI_APPS_DIR := Apps

DISK_IMAGE := $(BUILD_DIR)/$(PROJECT).img
DISK_IMAGE_SIZE := 1G
ISO_ROOT := $(BUILD_DIR)/iso-root
ISO_IMAGE := $(BUILD_DIR)/$(PROJECT).iso
ISO_BOOT_IMAGE := $(ISO_ROOT)/boot/$(PROJECT).img

PARTITION_LBA := 2048
PARTITION_OFFSET := 1048576
FAT_IMAGE := $(DISK_IMAGE)@@$(PARTITION_OFFSET)
FAT_IMAGE_ABS := $(abspath $(DISK_IMAGE))@@$(PARTITION_OFFSET)

BOOTSTRAP_IMAGE := $(BOOT_BUILD_DIR)/bootstrap.bin
STAGE2_IMAGE := $(BOOT_BUILD_DIR)/stage2.bin
KERNEL_IMAGE := $(KERNEL_BUILD_DIR)/kernel.elf
BOOT_CONFIG := $(BOOTLOADER_DIR)/AvBoot.conf

MKISOFS ?= xorriso -as mkisofs
QEMU ?= qemu-system-x86_64
PARTED ?= parted
MKFS_FAT ?= mkfs.fat
MCOPY ?= mcopy
MMD ?= mmd
DD ?= dd
RM ?= rm -f

.PHONY: all iso image boot bootloader kernel run clean FORCE

all: iso image
iso: $(ISO_IMAGE)
image: $(DISK_IMAGE)
boot bootloader: $(BOOTSTRAP_IMAGE) $(STAGE2_IMAGE)
kernel: $(KERNEL_IMAGE)
userspace:
	$(MAKE) -C $(LIBC_DIR)
	$(MAKE) -C $(ADK_DIR)
	$(MAKE) -C $(AETHER_DIR)
	$(MAKE) -C $(SRC_APPS_DIR)
	$(MAKE) -C $(GUI_APPS_DIR)

$(BOOTSTRAP_IMAGE) $(STAGE2_IMAGE) &: FORCE
	$(MAKE) -C $(BOOTLOADER_DIR)

$(KERNEL_IMAGE): FORCE
	$(MAKE) -C $(KERNEL_DIR)

$(DISK_IMAGE): $(BOOTSTRAP_IMAGE) $(STAGE2_IMAGE) $(KERNEL_IMAGE) $(BOOT_CONFIG) FORCE
	mkdir -p $(BUILD_DIR)
	rm -rf $(APP_STAGING_DIR)
	$(MAKE) -C $(AETHER_DIR) install DESTDIR="$(abspath $(APP_STAGING_DIR))" PREFIX="/System"
	$(MAKE) -C $(SRC_APPS_DIR) install DESTDIR="$(abspath $(APP_STAGING_DIR))"
	$(MAKE) -C $(GUI_APPS_DIR) install DESTDIR="$(abspath $(APP_STAGING_DIR))" PREFIX="/Applications"
	truncate -s $(DISK_IMAGE_SIZE) $@
	$(PARTED) -s $@ mklabel msdos
	$(PARTED) -s $@ unit s mkpart primary fat32 $(PARTITION_LBA)s 100%
	$(PARTED) -s $@ set 1 boot on
	$(DD) if=$(BOOTSTRAP_IMAGE) of=$@ bs=446 count=1 conv=notrunc status=none
	$(DD) if=$(STAGE2_IMAGE) of=$@ bs=512 seek=1 conv=notrunc status=none
	$(MKFS_FAT) -F 32 -s 1 -h $(PARTITION_LBA) -n $(ISO_LABEL) --offset=$(PARTITION_LBA) $@
	-$(MMD) -i $(FAT_IMAGE) ::Boot
	$(MCOPY) -i $(FAT_IMAGE) $(KERNEL_IMAGE) ::Boot/AvKernel.elf
	$(MCOPY) -i $(FAT_IMAGE) $(BOOT_CONFIG) ::Boot/AvBoot.conf
	for f in $(APP_STAGING_DIR)/*; do $(MCOPY) -i $(FAT_IMAGE) -s "$$f" ::; done
	for f in $(ROOTFS_DIR)/*; do $(MCOPY) -i $(FAT_IMAGE) -s "$$f" ::; done

$(ISO_BOOT_IMAGE): $(DISK_IMAGE)
	mkdir -p $(dir $@)
	cp $< $@

$(ISO_IMAGE): $(ISO_BOOT_IMAGE)
	$(MKISOFS) -quiet -V $(ISO_LABEL) -o $@ -b boot/$(PROJECT).img -hard-disk-boot $(ISO_ROOT)

run: $(DISK_IMAGE)
	$(QEMU) -drive file=$(DISK_IMAGE),format=raw,if=ide,index=0,media=disk -m 2G -serial stdio -vga std

clean:
	$(MAKE) -C $(BOOTLOADER_DIR) clean
	$(MAKE) -C $(KERNEL_DIR) clean
	$(MAKE) -C $(AETHER_DIR) clean
	$(MAKE) -C $(GUI_APPS_DIR) clean
	$(MAKE) -C $(SRC_APPS_DIR) clean
	$(MAKE) -C $(ADK_DIR) clean
	$(MAKE) -C $(LIBC_DIR) clean
	rm -rf $(BUILD_DIR)
