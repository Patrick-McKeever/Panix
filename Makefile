# Command line options.
SHELL := /bin/bash
DEBUG			=		0
DISPLAY			=		0
SHUTDOWN		=		0
MONITOR			=		0

CC 				:= 		gcc
AS				:= 		nasm
LD 				:= 		ld

CFLAGS 			:= 				 		\
	-std=gnu++20		 				\
	-fms-extensions						\
	-pipe -g		 					\
	-masm=intel							\
	-ffreestanding       				\
	-fno-exceptions						\
	-fno-rtti							\
	-fno-stack-protector 				\
	-fno-pic             				\
	-m64								\
	-mno-80387           				\
	-mno-mmx             				\
	-mno-3dnow           				\
	-fpie				 				\
	-lgcc				 				\
	-mno-sse			 				\
	-mno-sse2			 				\
	-Wall				 				\
	-Wextra								\
	-fno-omit-frame-pointer 			\
	-mno-red-zone

ifeq ($(DEBUG), 1)
	CFLAGS 		+=	-O0 -ggdb -DDEBUG
	BUILD_DIR	:=	./build/debug_bin
else
	CFLAGS 		+=	-O3
	BUILD_DIR	:=	./build/bin
endif

ASFLAGS 		:= 						\
	-felf64

LDFLAGS 		:= 						\
	-Tbuild/linker.ld					\
	-nostdlib

XORISSOFLAGS 	:= 						\
	-as mkisofs 						\
	-b limine-cd.bin 					\
    -no-emul-boot 						\
	-boot-load-size 4 					\
	-boot-info-table 					\
    --efi-boot limine-eltorito-efi.bin 	\
    -efi-boot-part 						\
	--efi-boot-image 					\
	--protective-msdos-label

QEMUFLAGS 		:= 						\
	-m 2G 								\
	-d int 								\
	-M smm=off 							\
	-D ./build/QemuLog.txt 				\
	-serial file:./build/KernLog.txt	\
	-smp 2								\
	-machine q35

ifeq ($(DISPLAY), 0)
	QEMUFLAGS 	+= 	-display none -debugcon stdio
endif

ifeq ($(SHUTDOWN), 0)
	QEMUFLAGS 	+=	-no-shutdown -no-reboot
endif

ifeq ($(MONITOR), 1)
	QEMUFLAGS	+=	-monitor stdio
endif

ifeq ($(AHCI), 1)
	QEMUFLAGS	+=	-trace ahci*
endif

ifeq ($(NCQ), 1)
	QEMUFLAGS	+=	-trace process_ncq_command* -trace ncq_finish -trace execute_ncq_command* -trace dma_blk_io \
					-trace ahci_dma*
endif

SRC_DIRS		:=						\
	ds									\
	sys									\
	libc

ISO_BOOT_FILES	:=						\
	build/limine/limine.cfg 			\
	build/limine/limine.sys				\
	build/limine/limine-cd.bin			\
	build/limine/limine-eltorito-efi.bin

KERN_CFILES 	:= 		$(shell find $(SRC_DIRS) -type f -name '*.cpp')
KERN_ASMFILES 	:= 		$(shell find $(SRC_DIRS) -type f -name '*.asm')
KERN_HFILES		:=		$(shell find $(SRC_DIRS) -type f -name '*.h')
KERN_OBJ_C 		:= 		$(KERN_CFILES:%.cpp=$(BUILD_DIR)/%.o)
KERN_OBJ_ASM 	:= 		$(KERN_ASMFILES:%.asm=$(BUILD_DIR)/%.o)

KERNEL 			:= 		$(BUILD_DIR)/kernel.elf
IMG_HDD			:=		$(BUILD_DIR)/image.hdd
.PHONY			: 		all clean

all				: 		$(KERNEL)

$(KERNEL)		: 		$(KERN_OBJ_C) $(KERN_OBJ_ASM) $(KERN_HFILES)
						$(LD) $(LDFLAGS) $(KERN_OBJ_C) -o $@

$(BUILD_DIR)/%.o	: 	%.cpp
						mkdir -p $(dir $@)
						$(CC) $(CFLAGS) -I. -c $< -o $@

$(BUILD_DIR)/%.o	: 	%.asm
						mkdir -p $(dir $@)
						$(AS) $(ASFLAGS) $< -o $@

define MAKE_HDD
	cp -v $(1)/kernel.elf $(ISO_BOOT_FILES) build/iso_root
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(1)/image.hdd
	parted -s $(1)/image.hdd mklabel gpt
	parted -s $(1)/image.hdd mkpart ESP ext2 2048s 100%
	parted -s $(1)/image.hdd set 1 esp on
	build/limine/limine-install $(1)/image.hdd
	$(eval USED_LOOPBACK := $(shell sudo losetup -Pf --show $(1)/image.hdd))
	echo $(USED_LOOPBACK)
	sudo mkfs.ext2 -F $(USED_LOOPBACK)p1 -r 0
	mkdir -p ./build/img_mnt
	mount ${USED_LOOPBACK}p1 ./build/img_mnt
	mkdir -p ./build/img_mnt/EFI/BOOT
	sudo cp -v ./build/iso_root/* ./build/img_mnt/
	sudo cp -v ./build/limine/BOOTX64.EFI ./build/img_mnt/EFI/BOOT
	sudo mkdir -p ./build/img_mnt/test_dir/test_dir2/test_dir3
	sudo cp -v austen.txt ./build/img_mnt/test_dir/test_dir2/test_dir3/test.txt
	sudo touch ./build/img_mnt/empty.txt
	sudo mkdir ./build/img_mnt/empty_dir

	sync
	sudo umount ./build/img_mnt
	sudo losetup -d $(USED_LOOPBACK)
endef

run:
	if [ $(shell id -u) -ne 0 ]; then echo 'Must run as root' >&2; exit 1; fi

	$(call MAKE_HDD,build/bin)
	qemu-system-x86_64 -drive format=raw,file=$(IMG_HDD) $(QEMUFLAGS)



debug:
	if [ $(shell id -u) -ne 0 ]; then echo 'Must run as root' >&2; exit 1; fi

	$(call MAKE_HDD,build/debug_bin)
	gdb --command=./build/debug.gdb

clean:
	find $(BUILD_DIR) -type f -name "*.o" -delete
	find $(BUILD_DIR) -type f -name "*.iso" -delete
	find $(BUILD_DIR) -type f -name "*.elf" -delete